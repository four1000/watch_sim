#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/activity/activity_types.h"
#include "ap/core/time.h"
#include "ap/services/sensor_types.h"
#include "ap/services/services.h"

namespace ap {

// Everything the UI needs about the current activity, copied by value
// (the screens never hold a pointer into the session).
struct ActivitySnapshot {
    ActivityState state = ActivityState::Idle;
    uint32_t elapsedMs = 0;          // moving time
    uint32_t distanceDm = 0;
    uint16_t currentPaceSecKm = 0;   // 0 = unknown / standing
    uint16_t avgPaceSecKm = 0;
    uint8_t avgHr = 0;
    uint8_t maxHr = 0;
    uint16_t lapCount = 0;           // completed laps
    uint32_t lapElapsedMs = 0;       // current lap
    uint32_t lapDistanceDm = 0;
    LapRecord lastLap{};
    bool hasLastLap = false;
    bool gpsFix = false;
};

// Pure computation: no services, no events, no clock - only injected samples
// and timestamps. That is why it is the most heavily unit-tested class.
class ActivityMetrics {
public:
    static constexpr std::size_t kMaxStoredLaps = 32;   // older laps still counted, not stored
    static constexpr std::size_t kPaceWindow = 6;       // GPS samples for current pace
    static constexpr uint16_t kMaxAccuracyM = 50;       // worse fixes are ignored
    static constexpr uint32_t kMaxSpeedDmPerS = 150;    // 15 m/s: faster = GPS glitch
    static constexpr uint16_t kMaxPaceSecKm = 59 * 60 + 59;

    void reset();
    void start(TickMs now);
    void pause(TickMs now);
    void resume(TickMs now);
    LapRecord lap(TickMs now);  // closes the current lap and starts a new one

    void onGps(const GpsSample& s);  // ignored unless running
    void onHr(const HrSample& s);    // ignored unless running

    bool running() const { return m_running; }
    uint32_t elapsedMs(TickMs now) const;
    uint32_t distanceDm() const { return m_distanceDm; }
    uint16_t currentPaceSecKm() const;
    uint16_t avgPaceSecKm(TickMs now) const;
    uint16_t lapCount() const { return m_lapCount; }
    const LapRecord* storedLap(std::size_t i) const;  // nullptr when out of range
    std::size_t storedLaps() const;

    ActivitySummary summary(TickMs now) const;
    void fill(TickMs now, ActivitySnapshot& out) const;

    static uint16_t paceSecKm(uint32_t durationMs, uint32_t distanceDm);

private:
    struct PacePoint {
        TickMs t;
        uint32_t distanceDm;
    };
    void clearPaceWindow() { m_paceCount = 0; m_paceHead = 0; }

    bool m_running = false;
    uint32_t m_accumMs = 0;       // closed running segments
    TickMs m_segmentStart = 0;

    bool m_hasBase = false;       // odometer baseline valid
    uint32_t m_lastOdometerDm = 0;
    TickMs m_lastGpsT = 0;
    uint32_t m_distanceDm = 0;

    std::array<PacePoint, kPaceWindow> m_pace{};
    std::size_t m_paceHead = 0;
    std::size_t m_paceCount = 0;

    uint32_t m_hrSum = 0;
    uint32_t m_hrCount = 0;
    uint8_t m_hrMax = 0;
    uint32_t m_lapHrSum = 0;
    uint32_t m_lapHrCount = 0;

    uint16_t m_lapCount = 0;
    uint32_t m_lapStartElapsedMs = 0;
    uint32_t m_lapStartDistanceDm = 0;
    std::array<LapRecord, kMaxStoredLaps> m_laps{};
    LapRecord m_lastLap{};  // kept separately: still valid after m_laps is full
};

}  // namespace ap
