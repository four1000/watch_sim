#pragma once

#include <cstdint>

#include "ap/core/soft_timer.h"
#include "ap/event/event_bus.h"
#include "ap/input/input_router.h"
#include "ap/services/services.h"

namespace ap {

// First link of the chain. Double-press LIGHT toggles the key lock; while
// locked, every other gesture is swallowed here, so no screen has to know
// that a lock exists.
class KeyLockHandler final : public IInputHandler {
public:
    explicit KeyLockHandler(EventBus& bus) : m_bus(bus) {}

    bool handle(const GestureEvent& e) override {
        if (e.key == KeyId::Light && e.gesture == Gesture::Double) {
            m_locked = !m_locked;
            m_bus.publish(Event::makeKeyLock(e.timestampMs, m_locked));
            return true;
        }
        return m_locked;
    }
    const char* handlerName() const override { return "KeyLock"; }
    bool locked() const { return m_locked; }

private:
    EventBus& m_bus;
    bool m_locked = false;
};

// Last link of the chain: global fallbacks that apply on every screen unless
// a screen consumed the key first. LIGHT short toggles the backlight, which
// switches itself off after kTimeoutMs (soft timer, no blocking wait).
class BacklightHandler final : public IInputHandler, private ITimerListener {
public:
    static constexpr uint32_t kTimeoutMs = 8000;

    BacklightHandler(IBacklight& light, SoftTimerPool& timers) : m_light(light), m_timers(timers) {}

    bool handle(const GestureEvent& e) override {
        if (e.key != KeyId::Light || e.gesture != Gesture::Short) return false;
        set(!m_on, e.timestampMs);
        return true;
    }
    const char* handlerName() const override { return "Backlight"; }
    bool on() const { return m_on; }

private:
    void set(bool on, TickMs now) {
        m_on = on;
        m_light.setOn(on);
        m_timers.cancel(m_timer);
        if (on) m_timer = m_timers.start(now, kTimeoutMs, *this, 0);
    }
    void onTimer(uint8_t, TickMs now) override { set(false, now); }

    IBacklight& m_light;
    SoftTimerPool& m_timers;
    TimerHandle m_timer{};
    bool m_on = false;
};

}  // namespace ap
