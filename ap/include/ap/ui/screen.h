#pragma once

#include "ap/core/time.h"
#include "ap/input/input_types.h"
#include "ap/ui/canvas.h"
#include "ap/ui/ui_model.h"

namespace ap {

class Screen {
public:
    void enter(TickMs now) { m_enteredAt = now; onEnter(now); }
    void exit(TickMs now) { onExit(now); }
    bool handle(const GestureEvent& e) { return onGesture(e); }

    void render(ICanvas& c, const UiModel& m) {
        c.clear(Color::Black);
        onRender(c, m);
        if (showsStatusBar()) drawStatusBar(c, m, showsClock());
    }

    virtual const char* name() const = 0;

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

protected:
    Screen() = default;
    ~Screen() = default;

    virtual void onEnter(TickMs) {}
    virtual void onExit(TickMs) {}
    virtual bool onGesture(const GestureEvent& e) = 0;
    virtual void onRender(ICanvas& c, const UiModel& m) = 0;
    virtual bool showsStatusBar() const { return true; }
    virtual bool showsClock() const { return true; }

    TickMs enteredAt() const { return m_enteredAt; }
    static void drawListPosition(ICanvas& c, std::size_t index, std::size_t count);

private:
    static void drawStatusBar(ICanvas& c, const UiModel& m, bool clock);
    TickMs m_enteredAt = 0;
};
}
