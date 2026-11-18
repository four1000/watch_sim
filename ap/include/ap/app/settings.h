#pragma once

#include <cstdint>

#include "ap/core/time.h"
#include "ap/event/event_bus.h"

namespace ap {

struct UserSettings {
    uint8_t maxHr = 190;
    uint8_t targetZone = 2;  // 1..5
    bool hrAlerts = true;

    static constexpr uint8_t kMinMaxHr = 120;
    static constexpr uint8_t kMaxMaxHr = 220;
};

// What the Settings screen may do (ISP: it cannot publish arbitrary events).
class ISettingsEditor {
public:
    virtual const UserSettings& settings() const = 0;
    virtual bool setMaxHr(uint8_t bpm, TickMs now) = 0;       // false: out of range
    virtual bool setTargetZone(uint8_t zone, TickMs now) = 0; // false: not 1..5
    virtual void setHrAlerts(bool on, TickMs now) = 0;

protected:
    ~ISettingsEditor() = default;
};

// Owns the settings and announces every effective change on the bus, so
// consumers (HrZoneMonitor) re-read instead of being called directly.
class SettingsStore final : public ISettingsEditor {
public:
    explicit SettingsStore(EventBus& bus) : m_bus(bus) {}

    const UserSettings& settings() const override { return m_settings; }
    bool setMaxHr(uint8_t bpm, TickMs now) override {
        if (bpm < UserSettings::kMinMaxHr || bpm > UserSettings::kMaxMaxHr) return false;
        if (bpm != m_settings.maxHr) { m_settings.maxHr = bpm; changed(now); }
        return true;
    }
    bool setTargetZone(uint8_t zone, TickMs now) override {
        if (zone < 1 || zone > 5) return false;
        if (zone != m_settings.targetZone) { m_settings.targetZone = zone; changed(now); }
        return true;
    }
    void setHrAlerts(bool on, TickMs now) override {
        if (on != m_settings.hrAlerts) { m_settings.hrAlerts = on; changed(now); }
    }

private:
    void changed(TickMs now) { m_bus.publish(Event::makeSettingsChanged(now)); }

    EventBus& m_bus;
    UserSettings m_settings;
};

}  // namespace ap
