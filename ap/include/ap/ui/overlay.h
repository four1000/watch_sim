#pragma once

#include <cstdint>

#include "ap/core/soft_timer.h"
#include "ap/event/event_bus.h"
#include "ap/input/input_router.h"
#include "ap/services/services.h"
#include "ap/ui/canvas.h"
#include "ap/ui/ui_model.h"

namespace ap {

class OverlayManager final : public IInputHandler, private IEventListener, private ITimerListener {
public:
    static constexpr uint32_t kToastMs = 1000;
    static constexpr uint32_t kAlertMs = 1000;

    OverlayManager(EventBus& bus, SoftTimerPool& timers, IVibrator& vibrator)
        : m_bus(bus), m_timers(timers), m_vibrator(vibrator) {}
    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;

    void attach();

    bool handle(const GestureEvent& e) override;
    const char* handlerName() const override { return "Overlay"; }

    void render(ICanvas& c, const UiModel& m) const;

    bool visible() const { return m_visible; }
    bool modal() const { return m_visible && m_modal; }
    const char* title() const { return m_visible ? m_title : ""; }

private:
    void onEvent(const Event& e) override;
    void onTimer(uint8_t tag, TickMs now) override;

    void show(TickMs now, const char* title, const char* detail, Color color, bool modal, uint32_t durationMs);
    void hide();

    EventBus& m_bus;
    SoftTimerPool& m_timers;
    IVibrator& m_vibrator;

    bool m_visible = false;
    bool m_modal = false;
    Color m_color = Color::DarkGray;
    char m_title[20] = {};
    char m_detail[28] = {};
    TimerHandle m_timer{};

    Subscription m_sub;
};
}
