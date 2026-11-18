#pragma once

#include "ap/core/time.h"
#include "ap/input/input_types.h"
#include "ap/ui/canvas.h"
#include "ap/ui/ui_model.h"

namespace ap {

// Base class for every screen, written as NVI (non-virtual interface) /
// Template Method:
//
//   the *public* functions are non-virtual and fix the order of steps
//   (clear background -> screen content -> status bar), the *protected*
//   virtual hooks are what each screen customises.
//
// That guarantees every screen gets the same chrome and lifecycle without
// each one remembering to call a base implementation.
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

    // ---- hooks ---------------------------------------------------------------
    virtual void onEnter(TickMs) {}
    virtual void onExit(TickMs) {}
    // true = consumed. Not consumed BACK pops the screen (Navigator default).
    virtual bool onGesture(const GestureEvent& e) = 0;
    virtual void onRender(ICanvas& c, const UiModel& m) = 0;
    virtual bool showsStatusBar() const { return true; }
    // Time of day in the status bar. Off inside an activity or settings, where
    // the space belongs to the task; the GPS / HR / lock indicators stay.
    virtual bool showsClock() const { return true; }

    TickMs enteredAt() const { return m_enteredAt; }

private:
    static void drawStatusBar(ICanvas& c, const UiModel& m, bool clock);
    TickMs m_enteredAt = 0;
};

}  // namespace ap
