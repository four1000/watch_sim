#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include "ap/services/osal.h"
#include "ap/services/services.h"

namespace sim {

class SimTick {
public:
    explicit SimTick(uint32_t startTick = 0) : m_start(std::chrono::steady_clock::now()), m_offset(startTick) {}
    ap::TickMs now() const {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start);
        return static_cast<ap::TickMs>(m_offset + static_cast<uint32_t>(ms.count()));
    }

private:
    std::chrono::steady_clock::time_point m_start;
    uint32_t m_offset;
};

class StdMutex final : public ap::osal::IMutex {
public:
    void lock() override { m_m.lock(); }
    void unlock() override { m_m.unlock(); }

private:
    std::mutex m_m;
};

class SimWallClock final : public ap::IWallClock {
public:
    SimWallClock() = default;
    SimWallClock(const ap::LocalTime& base, const ap::TickMs* tick) : m_fixed(true), m_base(base), m_tick(tick) {}

    ap::LocalTime now() const override;

private:
    bool m_fixed = false;
    ap::LocalTime m_base{};
    const ap::TickMs* m_tick = nullptr;
};

class SimVibrator final : public ap::IVibrator {
public:
    void vibrate(ap::VibePattern p) override {
        m_pattern = p;
        m_count.fetch_add(1);
    }
    uint32_t count() const { return m_count.load(); }
    ap::VibePattern last() const { return m_pattern.load(); }

private:
    std::atomic<ap::VibePattern> m_pattern{ap::VibePattern::Short};
    std::atomic<uint32_t> m_count{0};
};

class SimBacklight final : public ap::IBacklight {
public:
    void setOn(bool on) override { m_on = on; }
    bool on() const { return m_on.load(); }

private:
    std::atomic<bool> m_on{false};
};

class CsvActivityRecorder final : public ap::IActivityRecorder, public ap::IActivityHistory {
public:
    CsvActivityRecorder(std::string dir, const ap::IWallClock& clock) : m_dir(std::move(dir)), m_clock(clock) {}

    std::size_t loadExisting();

    bool begin(ap::TickMs startMs) override;
    bool recordLap(const ap::LapRecord& lap) override;
    bool finish(const ap::ActivitySummary& summary) override;
    void discard() override;

    std::size_t activityCount() const override;
    bool activityAt(std::size_t indexFromNewest, ap::ActivityRecord& out) const override;
    bool lapAt(std::size_t indexFromNewest, std::size_t lapIndex, ap::LapRecord& out) const override;

    std::atomic<bool> failBegin{false};
    std::atomic<bool> failFinish{false};
    std::string lastFile() const;

private:
    struct Stored {
        ap::ActivityRecord record;
        std::vector<ap::LapRecord> laps;
    };
    const Stored* stored(std::size_t indexFromNewest) const;

    std::string m_dir;
    const ap::IWallClock& m_clock;
    std::vector<ap::LapRecord> m_laps;
    std::vector<Stored> m_history;
    ap::LocalTime m_startTime{};
    bool m_open = false;
    mutable std::mutex m_fileMutex;
    std::string m_lastFile;
};
}
