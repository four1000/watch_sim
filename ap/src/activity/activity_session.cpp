#include "ap/activity/activity_session.h"

namespace ap {

ActivitySession::ActivitySession(IGpsService& gps, IActivityRecorder& recorder, EventBus& bus)
    : m_gps(gps), m_recorder(recorder), m_bus(bus) {}

void ActivitySession::attach() {
    m_sub = m_bus.subscribe(*this, topics(EventType::GpsSample, EventType::HrSample));
}

// ---- Public requests: delegate to the current state -----------------------------

bool ActivitySession::prepare(TickMs now) { return m_state->prepare(*this, now); }
bool ActivitySession::cancel(TickMs now) { return m_state->cancel(*this, now); }
bool ActivitySession::startPause(TickMs now) { return m_state->startPause(*this, now); }
bool ActivitySession::lap(TickMs now) { return m_state->lap(*this, now); }
bool ActivitySession::save(TickMs now) { return m_state->save(*this, now); }
bool ActivitySession::discard(TickMs now) { return m_state->discard(*this, now); }
ActivityState ActivitySession::state() const { return m_state->id(); }

void ActivitySession::onEvent(const Event& e) {
    if (e.type == EventType::GpsSample) {
        if (e.gps.hasFix) {
            m_gpsFix = true;
            m_lastFixMs = e.gps.timestampMs;
        }
        m_state->onGps(*this, e.gps);
    } else if (e.type == EventType::HrSample) {
        m_state->onHr(*this, e.hr);
    }
}

ActivitySnapshot ActivitySession::snapshot(TickMs now) const {
    ActivitySnapshot snap{};
    snap.state = state();
    if (snap.state == ActivityState::Running || snap.state == ActivityState::Paused) {
        m_metrics.fill(now, snap);
    }
    snap.gpsFix = snap.state != ActivityState::Idle && m_gpsFix &&
                  elapsedMs(now, m_lastFixMs) <= kGpsStaleMs;
    return snap;
}

void ActivitySession::transitionTo(State& next, TickMs now, ActivityOutcome outcome) {
    const ActivityState from = m_state->id();
    m_state->onExit(*this, now);
    m_state = &next;
    m_state->onEnter(*this, now);
    m_bus.publish(Event::makeActivity(now, from, next.id(), outcome));
}

void ActivitySession::publishFailure(TickMs now, ActivityOutcome outcome) {
    const ActivityState s = m_state->id();
    m_bus.publish(Event::makeActivity(now, s, s, outcome));
}

// ---- Idle ----------------------------------------------------------------------

void ActivitySession::IdleState::onEnter(ActivitySession& s, TickMs) {
    s.m_gps.stop();  // single place that turns GPS off, whatever path led here
    s.m_gpsFix = false;
}

bool ActivitySession::IdleState::prepare(ActivitySession& s, TickMs now) {
    if (!s.m_gps.start()) {
        s.publishFailure(now, ActivityOutcome::StartFailed);
        return false;
    }
    s.transitionTo(s.m_ready, now);
    return true;
}

// ---- Ready ---------------------------------------------------------------------

bool ActivitySession::ReadyState::cancel(ActivitySession& s, TickMs now) {
    s.transitionTo(s.m_idle, now, ActivityOutcome::Cancelled);
    return true;
}

bool ActivitySession::ReadyState::startPause(ActivitySession& s, TickMs now) {
    if (!s.m_recorder.begin(now)) {
        s.publishFailure(now, ActivityOutcome::StartFailed);
        return false;
    }
    s.m_metrics.start(now);
    s.transitionTo(s.m_running, now);
    return true;
}

// ---- Running -------------------------------------------------------------------

bool ActivitySession::RunningState::startPause(ActivitySession& s, TickMs now) {
    s.m_metrics.pause(now);
    s.transitionTo(s.m_paused, now);
    return true;
}

bool ActivitySession::RunningState::lap(ActivitySession& s, TickMs now) {
    const LapRecord rec = s.m_metrics.lap(now);
    s.m_recorder.recordLap(rec);
    s.m_bus.publish(Event::makeLap(now, rec));
    return true;
}

void ActivitySession::RunningState::onGps(ActivitySession& s, const GpsSample& g) { s.m_metrics.onGps(g); }
void ActivitySession::RunningState::onHr(ActivitySession& s, const HrSample& h) { s.m_metrics.onHr(h); }

// ---- Paused --------------------------------------------------------------------

bool ActivitySession::PausedState::startPause(ActivitySession& s, TickMs now) {
    s.m_metrics.resume(now);
    s.transitionTo(s.m_running, now);
    return true;
}

bool ActivitySession::PausedState::save(ActivitySession& s, TickMs now) {
    // Close the final partial lap so the file's laps add up to the total.
    ActivitySnapshot snap{};
    s.m_metrics.fill(now, snap);
    if (snap.lapElapsedMs > 0 || s.m_metrics.lapCount() == 0) {
        s.m_recorder.recordLap(s.m_metrics.lap(now));
    }
    if (!s.m_recorder.finish(s.m_metrics.summary(now))) {
        // Stay paused: the user can retry Save or Discard. Nothing is lost.
        s.publishFailure(now, ActivityOutcome::SaveFailed);
        return false;
    }
    s.transitionTo(s.m_idle, now, ActivityOutcome::Saved);
    return true;
}

bool ActivitySession::PausedState::discard(ActivitySession& s, TickMs now) {
    s.m_recorder.discard();
    s.transitionTo(s.m_idle, now, ActivityOutcome::Discarded);
    return true;
}

}  // namespace ap
