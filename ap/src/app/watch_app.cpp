#include "ap/app/watch_app.h"

#include <cstdint>

#include "ap/ui/themed_canvas.h"

namespace ap {

WatchApp::WatchApp(const Services& s)
    : m_svc(s),
      m_settings(m_bus),
      m_session(s.gps, s.recorder, m_bus),
      m_zones(m_bus, m_settings),
      m_watchFace(m_nav, m_activityMenu),
      m_dataScreen(m_session),
      m_settingsScreen(m_settings),
      m_detailScreen(s.history),
      m_historyScreen(s.history, m_nav, m_detailScreen),
      m_cmdRun(m_session, &IActivityControl::prepare),
      m_cmdHistory(m_nav, m_historyScreen),
      m_cmdSettings(m_nav, m_settingsScreen),
      m_cmdResume(m_session, &IActivityControl::startPause),
      m_cmdSave(m_session, &IActivityControl::save),
      m_cmdDiscard(m_session, &IActivityControl::discard),
      m_keyLock(m_bus),
      m_overlay(m_bus, m_timers, s.vibrator),
      m_backlight(s.backlight, m_timers),
      m_flow(m_bus, m_nav, m_watchFace, m_dataScreen, m_pauseMenu, m_detailScreen) {}

bool WatchApp::init(TickMs now) {
    if (m_initialized) return false;

    m_session.attach();
    m_zones.attach();
    m_overlay.attach();
    m_flow.attach();
    m_sub = m_bus.subscribe(*this, topics(EventType::Gesture, EventType::SecondTick));

    m_activityMenu.addItem("Run", m_cmdRun);
    m_activityMenu.addItem("History", m_cmdHistory);
    m_activityMenu.addItem("Settings", m_cmdSettings);
    m_pauseMenu.addItem("Resume", m_cmdResume);
    m_pauseMenu.addItem("Save", m_cmdSave);
    m_pauseMenu.addItem("Discard", m_cmdDiscard);

    m_router.append(m_keyLock);
    m_router.append(m_overlay);
    m_router.append(m_nav);
    m_router.append(m_backlight);

    m_gestures.setDoubleEnabled(KeyId::Light, true);

    m_nav.resetTo(m_watchFace, now);
    m_svc.hr.start();

    m_now = now;
    m_nextSecond = static_cast<TickMs>(now + 1000u);
    m_initialized = true;
    m_dirty = true;
    return true;
}

bool WatchApp::post(const InboxMsg& m) {
    osal::LockGuard lock(m_svc.inboxMutex);
    if (m_inbox.push(m)) return true;
    if (m_inboxDropped != UINT16_MAX) ++m_inboxDropped;
    return false;
}

bool WatchApp::postKey(const KeyEvent& e) {
    InboxMsg m{};
    m.kind = InboxMsg::Kind::Key;
    m.key = e;
    return post(m);
}

bool WatchApp::postHr(const HrSample& s) {
    InboxMsg m{};
    m.kind = InboxMsg::Kind::Hr;
    m.hr = s;
    return post(m);
}

bool WatchApp::postGps(const GpsSample& s) {
    InboxMsg m{};
    m.kind = InboxMsg::Kind::Gps;
    m.gps = s;
    return post(m);
}

uint16_t WatchApp::inboxDropped() const {
    osal::LockGuard lock(m_svc.inboxMutex);
    return m_inboxDropped;
}

void WatchApp::drainInbox() {
    InboxMsg batch[kMaxInboxPerStep];
    std::size_t n = 0;
    {
        osal::LockGuard lock(m_svc.inboxMutex);
        while (n < kMaxInboxPerStep) {
            const auto m = m_inbox.pop();
            if (!m) break;
            batch[n++] = *m;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        const InboxMsg& m = batch[i];
        switch (m.kind) {
            case InboxMsg::Kind::Key: m_gestures.onKeyEvent(m.key); break;
            case InboxMsg::Kind::Hr:  m_bus.publish(Event::makeHr(m.hr)); break;
            case InboxMsg::Kind::Gps: m_bus.publish(Event::makeGps(m.gps)); break;
        }
    }
}

void WatchApp::step(TickMs now) {
    if (!m_initialized) return;
    m_now = now;

    drainInbox();

    m_gestures.poll(now);
    while (const auto g = m_gestures.next()) m_bus.publish(Event::makeGesture(*g));

    m_timers.poll(now);
    if (m_backlight.on() != m_model.backlightOn) m_dirty = true;

    if (timeReached(now, m_nextSecond)) {
        m_bus.publish(Event::makeSecondTick(now));
        m_nextSecond = static_cast<TickMs>(m_nextSecond + 1000u);
        if (timeReached(now, m_nextSecond)) m_nextSecond = static_cast<TickMs>(now + 1000u);
    }

    if (m_bus.dispatch() > 0) m_dirty = true;
}

void WatchApp::onEvent(const Event& e) {
    if (e.type == EventType::Gesture) m_router.route(e.gesture);
    m_dirty = true;
}

void WatchApp::buildModel() {
    m_model.nowMs = m_now;
    m_model.time = m_svc.clock.now();
    m_model.activity = m_session.snapshot(m_now);
    m_model.hrBpm = m_zones.bpm();
    m_model.hrValid = m_zones.valid();
    m_model.hrZone = m_zones.zone();
    m_model.settings = m_settings.settings();
    m_model.keyLocked = m_keyLock.locked();
    m_model.backlightOn = m_backlight.on();
}

void WatchApp::render(ICanvas& c) {
    buildModel();
    ThemedCanvas themed(c, m_model.backlightOn ? Theme::Light : Theme::Dark);
    m_nav.render(themed, m_model);
    m_overlay.render(themed, m_model);
    m_dirty = false;
}

const char* WatchApp::topScreenName() const {
    const Screen* s = m_nav.top();
    return s ? s->name() : "-";
}

}
