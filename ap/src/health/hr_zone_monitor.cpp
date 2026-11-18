#include "ap/health/hr_zone_monitor.h"

#include <algorithm>

namespace ap {

uint8_t hrZoneLowerBound(uint8_t zone, uint8_t maxHr) {
    if (zone == 0) return 0;
    const unsigned pct = 40u + 10u * std::min<unsigned>(zone, kZoneCount);  // Z1=50% ... Z5=90%
    return static_cast<uint8_t>((maxHr * pct + 50u) / 100u);
}

uint8_t hrZoneFor(uint8_t bpm, uint8_t maxHr) {
    uint8_t z = 0;
    for (uint8_t candidate = 1; candidate <= kZoneCount; ++candidate) {
        if (bpm >= hrZoneLowerBound(candidate, maxHr)) z = candidate;
    }
    return z;
}

HrZoneMonitor::HrZoneMonitor(EventBus& bus, const ISettingsEditor& settings)
    : m_bus(bus), m_settings(settings) {}

void HrZoneMonitor::attach() {
    m_sub = m_bus.subscribe(*this, topics(EventType::HrSample, EventType::ActivityStateChanged,
                                          EventType::SettingsChanged));
}

void HrZoneMonitor::onEvent(const Event& e) {
    switch (e.type) {
        case EventType::HrSample:
            onSample(e.hr);
            break;
        case EventType::ActivityStateChanged:
            m_activityRunning = (e.activity.to == ActivityState::Running);
            if (!m_activityRunning) resetAlert();
            break;
        case EventType::SettingsChanged:
            // Thresholds moved: recompute without hysteresis and restart the debounce.
            if (m_valid) {
                const uint8_t z = hrZoneFor(m_bpm, m_settings.settings().maxHr);
                if (z != m_zone) {
                    m_zone = z;
                    m_bus.publish(Event::makeZone(e.timestampMs, m_zone, m_bpm));
                }
            }
            resetAlert();
            break;
        default:
            break;
    }
}

void HrZoneMonitor::onSample(const HrSample& s) {
    if (!s.valid) {
        // Sensor lost: no zone, and no alert based on stale data.
        m_valid = false;
        m_bpm = 0;
        resetAlert();
        if (m_zone != 0) {
            m_zone = 0;
            m_bus.publish(Event::makeZone(s.timestampMs, 0, 0));
        }
        return;
    }
    m_valid = true;
    m_bpm = s.bpm;
    updateZone(s.timestampMs);
    checkAlert(s.timestampMs);
}

void HrZoneMonitor::updateZone(TickMs t) {
    const uint8_t maxHr = m_settings.settings().maxHr;
    // Hysteresis: move up only if bpm clears the boundary by H, down only if it
    // falls below it by H. Inside the band the current zone is kept.
    const uint8_t upZone = hrZoneFor(static_cast<uint8_t>(m_bpm > kHysteresisBpm ? m_bpm - kHysteresisBpm : 0), maxHr);
    const uint8_t downZone = hrZoneFor(static_cast<uint8_t>(std::min(255, m_bpm + kHysteresisBpm)), maxHr);
    uint8_t z = m_zone;
    if (upZone > m_zone) z = upZone;
    else if (downZone < m_zone) z = downZone;
    if (z != m_zone) {
        m_zone = z;
        m_bus.publish(Event::makeZone(t, m_zone, m_bpm));
    }
}

void HrZoneMonitor::checkAlert(TickMs t) {
    const UserSettings& cfg = m_settings.settings();
    if (!m_activityRunning || !cfg.hrAlerts) {
        resetAlert();
        return;
    }
    if (m_zone == cfg.targetZone) {
        resetAlert();
        return;
    }
    if (!m_outOfTarget) {
        m_outOfTarget = true;
        m_outSince = t;
        return;
    }
    if (elapsedMs(t, m_outSince) < kDebounceMs) return;
    if (m_alerted && elapsedMs(t, m_lastAlert) < kRealertMs) return;

    m_alerted = true;
    m_lastAlert = t;
    const HrAlertInfo info{m_zone > cfg.targetZone ? HrAlertKind::AboveTarget : HrAlertKind::BelowTarget,
                           m_bpm, m_zone, cfg.targetZone};
    m_bus.publish(Event::makeAlert(t, info));
}

}  // namespace ap
