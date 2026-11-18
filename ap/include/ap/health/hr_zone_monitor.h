#pragma once

#include <cstdint>

#include "ap/app/settings.h"
#include "ap/event/event_bus.h"
#include "ap/health/hr_types.h"

namespace ap {

uint8_t hrZoneFor(uint8_t bpm, uint8_t maxHr);
uint8_t hrZoneLowerBound(uint8_t zone, uint8_t maxHr);

class HrZoneMonitor final : private IEventListener {
public:
    static constexpr uint8_t kHysteresisBpm = 2;
    static constexpr uint32_t kDebounceMs = 3000;
    static constexpr uint32_t kRealertMs = 30000;

    HrZoneMonitor(EventBus& bus, const ISettingsEditor& settings);
    HrZoneMonitor(const HrZoneMonitor&) = delete;
    HrZoneMonitor& operator=(const HrZoneMonitor&) = delete;

    void attach();

    uint8_t zone() const { return m_zone; }
    uint8_t bpm() const { return m_bpm; }
    bool valid() const { return m_valid; }

private:
    void onEvent(const Event& e) override;
    void onSample(const HrSample& s);
    void updateZone(TickMs t);
    void checkAlert(TickMs t);
    void resetAlert() { m_outOfTarget = false; m_alerted = false; }

    EventBus& m_bus;
    const ISettingsEditor& m_settings;

    uint8_t m_bpm = 0;
    bool m_valid = false;
    uint8_t m_zone = 0;
    bool m_activityRunning = false;

    bool m_outOfTarget = false;
    TickMs m_outSince = 0;
    bool m_alerted = false;
    TickMs m_lastAlert = 0;

    Subscription m_sub;
};
}
