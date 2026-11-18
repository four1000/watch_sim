#pragma once

#include <cstdint>

#include "ap/activity/activity_metrics.h"
#include "ap/activity/activity_types.h"
#include "ap/event/event_bus.h"
#include "ap/services/services.h"

namespace ap {

class IActivityControl {
public:
    virtual bool prepare(TickMs now) = 0;
    virtual bool cancel(TickMs now) = 0;
    virtual bool startPause(TickMs now) = 0;
    virtual bool lap(TickMs now) = 0;
    virtual bool save(TickMs now) = 0;
    virtual bool discard(TickMs now) = 0;
    virtual ActivityState state() const = 0;

protected:
    ~IActivityControl() = default;
};

class ActivitySession final : public IActivityControl, private IEventListener {
public:
    ActivitySession(IGpsService& gps, IActivityRecorder& recorder, EventBus& bus);
    ActivitySession(const ActivitySession&) = delete;
    ActivitySession& operator=(const ActivitySession&) = delete;

    void attach();

    bool prepare(TickMs now) override;
    bool cancel(TickMs now) override;
    bool startPause(TickMs now) override;
    bool lap(TickMs now) override;
    bool save(TickMs now) override;
    bool discard(TickMs now) override;
    ActivityState state() const override;

    ActivitySnapshot snapshot(TickMs now) const;
    const ActivityMetrics& metrics() const { return m_metrics; }

    static constexpr uint32_t kGpsStaleMs = 3000;

private:
    void onEvent(const Event& e) override;

    class State {
    public:
        virtual ActivityState id() const = 0;
        virtual void onEnter(ActivitySession&, TickMs) {}
        virtual void onExit(ActivitySession&, TickMs) {}
        virtual bool prepare(ActivitySession&, TickMs) { return false; }
        virtual bool cancel(ActivitySession&, TickMs) { return false; }
        virtual bool startPause(ActivitySession&, TickMs) { return false; }
        virtual bool lap(ActivitySession&, TickMs) { return false; }
        virtual bool save(ActivitySession&, TickMs) { return false; }
        virtual bool discard(ActivitySession&, TickMs) { return false; }
        virtual void onGps(ActivitySession&, const GpsSample&) {}
        virtual void onHr(ActivitySession&, const HrSample&) {}

    protected:
        ~State() = default;
    };
    class IdleState final : public State {
    public:
        ActivityState id() const override { return ActivityState::Idle; }
        void onEnter(ActivitySession& s, TickMs now) override;
        bool prepare(ActivitySession& s, TickMs now) override;
    };
    class ReadyState final : public State {
    public:
        ActivityState id() const override { return ActivityState::Ready; }
        bool cancel(ActivitySession& s, TickMs now) override;
        bool startPause(ActivitySession& s, TickMs now) override;
    };
    class RunningState final : public State {
    public:
        ActivityState id() const override { return ActivityState::Running; }
        bool startPause(ActivitySession& s, TickMs now) override;
        bool lap(ActivitySession& s, TickMs now) override;
        void onGps(ActivitySession& s, const GpsSample& g) override;
        void onHr(ActivitySession& s, const HrSample& h) override;
    };
    class PausedState final : public State {
    public:
        ActivityState id() const override { return ActivityState::Paused; }
        bool startPause(ActivitySession& s, TickMs now) override;
        bool save(ActivitySession& s, TickMs now) override;
        bool discard(ActivitySession& s, TickMs now) override;
    };

    void transitionTo(State& next, TickMs now, ActivityOutcome outcome = ActivityOutcome::None);
    void publishFailure(TickMs now, ActivityOutcome outcome);

    IGpsService& m_gps;
    IActivityRecorder& m_recorder;
    EventBus& m_bus;
    ActivityMetrics m_metrics;

    IdleState m_idle;
    ReadyState m_ready;
    RunningState m_running;
    PausedState m_paused;
    State* m_state = &m_idle;

    bool m_gpsFix = false;
    TickMs m_lastFixMs = 0;

    Subscription m_sub;
};
}
