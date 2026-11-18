#pragma once

// Simulator implementations of the AP service interfaces.
// This layer may use the full C++ library (threads, heap, iostreams):
// it stands in for drivers and the RTOS, not for application code.

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

// ---- Monotonic tick ---------------------------------------------------------------
// `startTick` lets the simulator start close to UINT32_MAX to show that the
// AP layer survives the 49-day tick wrap (--wrap).
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

// ---- OSAL --------------------------------------------------------------------------
class StdMutex final : public ap::osal::IMutex {
public:
    void lock() override { m_m.lock(); }
    void unlock() override { m_m.unlock(); }

private:
    std::mutex m_m;
};

// ---- Wall clock: real local time, or a fixed start advanced by the tick (scripts) ---
class SimWallClock final : public ap::IWallClock {
public:
    SimWallClock() = default;
    // Deterministic clock for scripted runs: base time + elapsed virtual ticks.
    SimWallClock(const ap::LocalTime& base, const ap::TickMs* tick) : m_fixed(true), m_base(base), m_tick(tick) {}

    ap::LocalTime now() const override;

private:
    bool m_fixed = false;
    ap::LocalTime m_base{};
    const ap::TickMs* m_tick = nullptr;
};

// ---- Outputs -------------------------------------------------------------------------
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

// ---- Storage: CSV file per activity ----------------------------------------------------
class CsvActivityRecorder final : public ap::IActivityRecorder {
public:
    explicit CsvActivityRecorder(std::string dir) : m_dir(std::move(dir)) {}

    bool begin(ap::TickMs startMs) override;
    bool recordLap(const ap::LapRecord& lap) override;
    bool finish(const ap::ActivitySummary& summary) override;
    void discard() override;

    // Fault injection from the dev panel.
    std::atomic<bool> failBegin{false};
    std::atomic<bool> failFinish{false};
    std::string lastFile() const;

private:
    std::string m_dir;
    std::vector<ap::LapRecord> m_laps;
    std::time_t m_started = 0;
    bool m_open = false;
    mutable std::mutex m_fileMutex;
    std::string m_lastFile;
};

}  // namespace sim
