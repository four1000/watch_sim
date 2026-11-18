#include "ap/activity/activity_metrics.h"

#include <algorithm>
#include <cstdint>

namespace ap {

const char* toString(ActivityState s) {
    switch (s) {
        case ActivityState::Idle:    return "Idle";
        case ActivityState::Ready:   return "Ready";
        case ActivityState::Running: return "Running";
        case ActivityState::Paused:  return "Paused";
    }
    return "?";
}

void ActivityMetrics::reset() { *this = ActivityMetrics{}; }

void ActivityMetrics::start(TickMs now) {
    reset();
    m_running = true;
    m_segmentStart = now;
}

void ActivityMetrics::pause(TickMs now) {
    if (!m_running) return;
    m_accumMs += ap::elapsedMs(now, m_segmentStart);
    m_running = false;
    clearPaceWindow();
}

void ActivityMetrics::resume(TickMs now) {
    if (m_running) return;
    m_running = true;
    m_segmentStart = now;
    m_hasBase = false;
}

uint32_t ActivityMetrics::elapsedMs(TickMs now) const {
    return m_running ? m_accumMs + ap::elapsedMs(now, m_segmentStart) : m_accumMs;
}

LapRecord ActivityMetrics::lap(TickMs now) {
    const uint32_t elapsed = elapsedMs(now);
    LapRecord rec{};
    rec.index = static_cast<uint16_t>(m_lapCount == UINT16_MAX ? m_lapCount : m_lapCount + 1);
    rec.durationMs = elapsed - m_lapStartElapsedMs;
    rec.distanceDm = m_distanceDm - m_lapStartDistanceDm;
    rec.avgHr = m_lapHrCount ? static_cast<uint8_t>(m_lapHrSum / m_lapHrCount) : 0;

    if (m_lapCount < kMaxStoredLaps) m_laps[m_lapCount] = rec;
    m_lastLap = rec;
    if (m_lapCount != UINT16_MAX) ++m_lapCount;
    m_lapStartElapsedMs = elapsed;
    m_lapStartDistanceDm = m_distanceDm;
    m_lapHrSum = m_lapHrCount = 0;
    return rec;
}

void ActivityMetrics::onGps(const GpsSample& s) {
    if (!m_running || !s.hasFix || s.accuracyM > kMaxAccuracyM) return;

    if (!m_hasBase) {
        m_hasBase = true;
        m_lastOdometerDm = s.odometerDm;
        m_lastGpsT = s.timestampMs;
    } else {
        const uint32_t delta = s.odometerDm - m_lastOdometerDm;
        const uint32_t dtMs = ap::elapsedMs(s.timestampMs, m_lastGpsT);
        const uint64_t limitDm = (static_cast<uint64_t>(dtMs) * kMaxSpeedDmPerS) / 1000u + 1u;
        if (delta <= limitDm) m_distanceDm += delta;
        m_lastOdometerDm = s.odometerDm;
        m_lastGpsT = s.timestampMs;
    }

    m_pace[m_paceHead] = PacePoint{s.timestampMs, m_distanceDm};
    m_paceHead = (m_paceHead + 1) % kPaceWindow;
    if (m_paceCount < kPaceWindow) ++m_paceCount;
}

void ActivityMetrics::onHr(const HrSample& s) {
    if (!m_running || !s.valid) return;
    m_hrSum += s.bpm;
    ++m_hrCount;
    m_lapHrSum += s.bpm;
    ++m_lapHrCount;
    m_hrMax = std::max(m_hrMax, s.bpm);
}

uint16_t ActivityMetrics::paceSecKm(uint32_t durationMs, uint32_t distanceDm) {
    if (distanceDm < 20) return 0;
    const uint64_t pace = (static_cast<uint64_t>(durationMs) * 10u) / distanceDm;
    return static_cast<uint16_t>(std::min<uint64_t>(pace, kMaxPaceSecKm));
}

uint16_t ActivityMetrics::currentPaceSecKm() const {
    if (m_paceCount < 2) return 0;
    const std::size_t newest = (m_paceHead + kPaceWindow - 1) % kPaceWindow;
    const std::size_t oldest = (m_paceHead + kPaceWindow - m_paceCount) % kPaceWindow;
    const PacePoint& a = m_pace[oldest];
    const PacePoint& b = m_pace[newest];
    return paceSecKm(ap::elapsedMs(b.t, a.t), b.distanceDm - a.distanceDm);
}

uint16_t ActivityMetrics::avgPaceSecKm(TickMs now) const { return paceSecKm(elapsedMs(now), m_distanceDm); }

const LapRecord* ActivityMetrics::storedLap(std::size_t i) const {
    return i < storedLaps() ? &m_laps[i] : nullptr;
}

std::size_t ActivityMetrics::storedLaps() const {
    return std::min<std::size_t>(m_lapCount, kMaxStoredLaps);
}

ActivitySummary ActivityMetrics::summary(TickMs now) const {
    ActivitySummary s{};
    s.elapsedMs = elapsedMs(now);
    s.distanceDm = m_distanceDm;
    s.laps = m_lapCount;
    s.avgHr = m_hrCount ? static_cast<uint8_t>(m_hrSum / m_hrCount) : 0;
    s.maxHr = m_hrMax;
    return s;
}

void ActivityMetrics::fill(TickMs now, ActivitySnapshot& out) const {
    const uint32_t elapsed = elapsedMs(now);
    out.elapsedMs = elapsed;
    out.distanceDm = m_distanceDm;
    out.currentPaceSecKm = m_running ? currentPaceSecKm() : 0;
    out.avgPaceSecKm = paceSecKm(elapsed, m_distanceDm);
    out.avgHr = m_hrCount ? static_cast<uint8_t>(m_hrSum / m_hrCount) : 0;
    out.maxHr = m_hrMax;
    out.lapCount = m_lapCount;
    out.lapElapsedMs = elapsed - m_lapStartElapsedMs;
    out.lapDistanceDm = m_distanceDm - m_lapStartDistanceDm;
    out.hasLastLap = m_lapCount > 0;
    out.lastLap = m_lastLap;
}

}
