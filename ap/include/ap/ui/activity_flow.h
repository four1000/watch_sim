#pragma once

#include "ap/event/event_bus.h"
#include "ap/ui/navigator.h"
#include "ap/ui/screens.h"

namespace ap {

class ActivityFlowCoordinator final : private IEventListener {
public:
    ActivityFlowCoordinator(EventBus& bus, Navigator& nav, Screen& home, Screen& dataScreen, Screen& pauseMenu,
                            ActivityDetailScreen& savedSummary)
        : m_bus(bus), m_nav(nav), m_home(home), m_data(dataScreen), m_pause(pauseMenu), m_summary(savedSummary) {}
    ActivityFlowCoordinator(const ActivityFlowCoordinator&) = delete;
    ActivityFlowCoordinator& operator=(const ActivityFlowCoordinator&) = delete;

    void attach() { m_sub = m_bus.subscribe(*this, topic(EventType::ActivityStateChanged)); }

private:
    static constexpr std::size_t kNewestActivity = 0;

    void onEvent(const Event& e) override {
        const ActivityStateChange& c = e.activity;
        if (c.from == c.to) return;
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
                if (c.outcome == ActivityOutcome::Saved) {
                    m_summary.select(kNewestActivity);
                    m_nav.push(m_summary, t);
                }
                break;
        }
    }

    EventBus& m_bus;
    Navigator& m_nav;
    Screen& m_home;
    Screen& m_data;
    Screen& m_pause;
    ActivityDetailScreen& m_summary;
    Subscription m_sub;
};
}
