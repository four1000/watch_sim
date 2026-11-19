#include "sim/sim_services.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace sim {
namespace {

ap::LocalTime fromTm(const std::tm& t) {
    ap::LocalTime lt{};
    lt.year = static_cast<uint16_t>(t.tm_year + 1900);
    lt.month = static_cast<uint8_t>(t.tm_mon + 1);
    lt.day = static_cast<uint8_t>(t.tm_mday);
    lt.weekday = static_cast<uint8_t>(t.tm_wday);
    lt.hour = static_cast<uint8_t>(t.tm_hour);
    lt.minute = static_cast<uint8_t>(t.tm_min);
    lt.second = static_cast<uint8_t>(t.tm_sec);
    return lt;
}
}

ap::LocalTime SimWallClock::now() const {
    if (!m_fixed) {
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        return fromTm(tm);
    }
    const uint32_t secs = (m_tick ? *m_tick : 0u) / 1000u;
    std::tm tm{};
    tm.tm_year = m_base.year - 1900;
    tm.tm_mon = m_base.month - 1;
    tm.tm_mday = m_base.day;
    tm.tm_hour = m_base.hour;
    tm.tm_min = m_base.minute;
    tm.tm_sec = m_base.second + static_cast<int>(secs);
    tm.tm_isdst = -1;
    std::mktime(&tm);
    return fromTm(tm);
}

namespace {
std::string fileNameFor(const ap::LocalTime& t) {
    char name[64];
    std::snprintf(name, sizeof name, "run_%04u%02u%02u_%02u%02u%02u.csv", static_cast<unsigned>(t.year),
                  static_cast<unsigned>(t.month), static_cast<unsigned>(t.day), static_cast<unsigned>(t.hour),
                  static_cast<unsigned>(t.minute), static_cast<unsigned>(t.second));
    return name;
}

bool startFromFileName(const std::string& name, ap::LocalTime& out) {
    std::tm tm{};
    if (std::sscanf(name.c_str(), "run_%4d%2d%2d_%2d%2d%2d.csv", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
                    &tm.tm_sec) != 6) {
        return false;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = -1;
    if (std::mktime(&tm) == -1) return false;
    out = fromTm(tm);
    return true;
}

uint32_t secondsToMs(double s) { return static_cast<uint32_t>(std::llround(s * 1000.0)); }
uint32_t metresToDm(double m) { return static_cast<uint32_t>(std::llround(m * 10.0)); }
}

bool CsvActivityRecorder::begin(ap::TickMs) {
    if (failBegin) return false;
    m_laps.clear();
    m_startTime = m_clock.now();
    m_open = true;
    return true;
}

bool CsvActivityRecorder::recordLap(const ap::LapRecord& lap) {
    if (!m_open) return false;
    m_laps.push_back(lap);
    return true;
}

bool CsvActivityRecorder::finish(const ap::ActivitySummary& s) {
    if (!m_open || failFinish) return false;
    std::error_code ec;
    std::filesystem::create_directories(m_dir, ec);
    const std::string path = (std::filesystem::path(m_dir) / fileNameFor(m_startTime)).string();

    std::ofstream f(path);
    if (!f) return false;
    f << "type,index,duration_s,distance_m,avg_hr,max_hr\n";
    for (const auto& l : m_laps) {
        f << "lap," << l.index << ',' << l.durationMs / 1000.0 << ',' << l.distanceDm / 10.0 << ','
          << static_cast<unsigned>(l.avgHr) << ",\n";
    }
    f << "summary," << s.laps << ',' << s.elapsedMs / 1000.0 << ',' << s.distanceDm / 10.0 << ','
      << static_cast<unsigned>(s.avgHr) << ',' << static_cast<unsigned>(s.maxHr) << '\n';
    f.close();
    if (!f) return false;

    m_history.push_back(Stored{ap::ActivityRecord{m_startTime, s}, std::move(m_laps)});
    m_laps.clear();
    m_open = false;
    std::lock_guard<std::mutex> lock(m_fileMutex);
    m_lastFile = path;
    return true;
}

void CsvActivityRecorder::discard() {
    m_laps.clear();
    m_open = false;
}

std::string CsvActivityRecorder::lastFile() const {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    return m_lastFile;
}

std::size_t CsvActivityRecorder::loadExisting() {
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(m_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".csv") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    std::size_t loaded = 0;
    for (const auto& file : files) {
        Stored stored{};
        if (!startFromFileName(file.filename().string(), stored.record.start)) continue;
        std::ifstream in(file);
        bool hasSummary = false;
        std::string line;
        while (std::getline(in, line)) {
            unsigned index = 0, avg = 0, max = 0;
            double seconds = 0, metres = 0;
            if (std::sscanf(line.c_str(), "lap,%u,%lf,%lf,%u", &index, &seconds, &metres, &avg) == 4) {
                stored.laps.push_back(ap::LapRecord{static_cast<uint16_t>(index), secondsToMs(seconds),
                                                    metresToDm(metres), static_cast<uint8_t>(avg)});
            } else if (std::sscanf(line.c_str(), "summary,%u,%lf,%lf,%u,%u", &index, &seconds, &metres, &avg, &max) ==
                       5) {
                stored.record.summary = ap::ActivitySummary{secondsToMs(seconds), metresToDm(metres),
                                                            static_cast<uint16_t>(index), static_cast<uint8_t>(avg),
                                                            static_cast<uint8_t>(max)};
                hasSummary = true;
            }
        }
        if (!hasSummary) continue;
        m_history.push_back(std::move(stored));
        ++loaded;
    }
    return loaded;
}

const CsvActivityRecorder::Stored* CsvActivityRecorder::stored(std::size_t indexFromNewest) const {
    if (indexFromNewest >= m_history.size()) return nullptr;
    return &m_history[m_history.size() - 1 - indexFromNewest];
}

std::size_t CsvActivityRecorder::activityCount() const { return m_history.size(); }

bool CsvActivityRecorder::activityAt(std::size_t indexFromNewest, ap::ActivityRecord& out) const {
    const Stored* s = stored(indexFromNewest);
    if (s == nullptr) return false;
    out = s->record;
    return true;
}

bool CsvActivityRecorder::lapAt(std::size_t indexFromNewest, std::size_t lapIndex, ap::LapRecord& out) const {
    const Stored* s = stored(indexFromNewest);
    if (s == nullptr || lapIndex >= s->laps.size()) return false;
    out = s->laps[lapIndex];
    return true;
}

}
