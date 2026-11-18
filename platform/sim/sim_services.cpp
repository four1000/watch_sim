#include "sim/sim_services.h"

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

bool CsvActivityRecorder::begin(ap::TickMs) {
    if (failBegin) return false;
    m_laps.clear();
    m_started = std::time(nullptr);
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

    std::tm tm{};
    localtime_s(&tm, &m_started);
    char name[64];
    std::snprintf(name, sizeof name, "run_%04d%02d%02d_%02d%02d%02d.csv", tm.tm_year + 1900, tm.tm_mon + 1,
                  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    const std::string path = (std::filesystem::path(m_dir) / name).string();

    std::ofstream f(path);
    if (!f) return false;
    f << "type,index,duration_s,distance_m,avg_hr,max_hr\n";
    for (const auto& l : m_laps) {
        f << "lap," << l.index << ',' << l.durationMs / 1000.0 << ',' << l.distanceDm / 10.0 << ','
          << static_cast<unsigned>(l.avgHr) << ",\n";
    }
    f << "summary," << s.laps << ',' << s.elapsedMs / 1000.0 << ',' << s.distanceDm / 10.0 << ','
      << static_cast<unsigned>(s.avgHr) << ',' << static_cast<unsigned>(s.maxHr) << '\n';
    m_open = false;
    std::lock_guard<std::mutex> lock(m_fileMutex);
    m_lastFile = path;
    return static_cast<bool>(f);
}

void CsvActivityRecorder::discard() {
    m_laps.clear();
    m_open = false;
}

std::string CsvActivityRecorder::lastFile() const {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    return m_lastFile;
}

}
