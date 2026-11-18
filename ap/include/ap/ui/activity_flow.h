#pragma once

#include "ap/event/event_bus.h"
#include "ap/ui/navigator.h"

namespace ap {

// Mediator between the activity session and the screen stack.
//
// The session only announces "Running -> Paused"; it has no idea a pause menu
// exists. Screens only call IActivityControl; they never push the pause menu
// themselves. This one class owns the rule "which screen follows which
// activity state", so changing the UX flow touches exactly one file.
class ActivityFlowCoordinator final : private IEventListener {
public:
    ActivityFlowCoordinator(EventBus& bus, Navigator& nav, Screen& home, Screen& dataScreen, Screen& pauseMenu)
        : m_bus(bus), m_nav(nav), m_home(home), m_data(dataScreen), m_pause(pauseMenu) {}
    ActivityFlowCoordinator(const ActivityFlowCoordinator&) = delete;
    ActivityFlowCoordinator& operator=(const ActivityFlowCoordinator&) = delete;

    void attach() { m_sub = m_bus.subscribe(*this, topic(EventType::ActivityStateChanged)); }

private:
    void onEvent(const Event& e) override {
        const ActivityStateChange& c = e.activity;
        if (c.from == c.to) return;  // failure notification, handled by the overlay
        const TickMs t = e.timestampMs;
        switch (c.to) {
            case ActivityState::Ready:
                m_nav.push(m_data, t);
                break;
            case ActivityState::Running:
                if (c.from == ActivityState::Paused) m_nav.popUntil(m_data, t);
                break;
            case ActivityState::Paused:
                m_nav.push(m_pause, t);
                break;
            case ActivityState::Idle:
                m_nav.resetTo(m_home, t);
                break;
        }
    }

    EventBus& m_bus;
    Navigator& m_nav;
    Screen& m_home;
    Screen& m_data;
    Screen& m_pause;
    Subscription m_sub;
};

}  // namespace ap
