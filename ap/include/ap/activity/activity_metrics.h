#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/activity/activity_types.h"
#include "ap/core/time.h"
#include "ap/services/sensor_types.h"
#include "ap/services/services.h"

namespace ap {

struct ActivitySnapshot {
    ActivityState state = ActivityState::Idle;
    uint32_t elapsedMs = 0;
    uint32_t distanceDm = 0;
    uint16_t currentPaceSecKm = 0;
    uint16_t avgPaceSecKm = 0;
    uint8_t avgHr = 0;
    uint8_t maxHr = 0;
    uint16_t lapCount = 0;
    uint32_t lapElapsedMs = 0;
    uint32_t lapDistanceDm = 0;
    LapRecord lastLap{};
    bool hasLastLap = false;
    bool gpsFix = false;
};

class ActivityMetrics {
public:
    static constexpr std::size_t kMaxStoredLaps = 32;
    static constexpr std::size_t kPaceWindow = 6;
    static constexpr uint16_t kMaxAccuracyM = 50;
    static constexpr uint32_t kMaxSpeedDmPerS = 150;
    static constexpr uint16_t kMaxPaceSecKm = 59 * 60 + 59;

    void reset();
    void start(TickMs now);
    void pause(TickMs now);
    void resume(TickMs now);
    LapRecord lap(TickMs now);

    void onGps(const GpsSample& s);
    void onHr(const HrSample& s);

    bool running() const { return m_running; }
    uint32_t elapsedMs(TickMs now) const;
    uint32_t distanceDm() const { return m_distanceDm; }
    uint16_t currentPaceSecKm() const;
    uint16_t avgPaceSecKm(TickMs now) const;
    uint16_t lapCount() const { return m_lapCount; }
    const LapRecord* storedLap(std::size_t i) const;
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
    uint32_t m_accumMs = 0;
    TickMs m_segmentStart = 0;

    bool m_hasBase = false;
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
    LapRecord m_lastLap{};
};

}
