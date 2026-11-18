#pragma once

#include <cstdint>

#include "ap/activity/activity_session.h"
#include "ap/app/settings.h"
#include "ap/core/ring_buffer.h"
#include "ap/core/soft_timer.h"
#include "ap/event/event_bus.h"
#include "ap/health/hr_zone_monitor.h"
#include "ap/input/gesture_recognizer.h"
#include "ap/input/input_router.h"
#include "ap/services/osal.h"
#include "ap/services/services.h"
#include "ap/ui/activity_flow.h"
#include "ap/ui/commands.h"
#include "ap/ui/navigator.h"
#include "ap/ui/overlay.h"
#include "ap/ui/screens.h"
#include "ap/ui/system_handlers.h"
#include "ap/ui/ui_model.h"

namespace ap {

class WatchApp final : public ISensorSink, private IEventListener {
public:
    struct Services {
        IGpsService& gps;
        IHeartRateService& hr;
        IActivityRecorder& recorder;
        IVibrator& vibrator;
        IBacklight& backlight;
        IWallClock& clock;
        osal::IMutex& inboxMutex;
    };

    static constexpr std::size_t kInboxCapacity = 32;
    static constexpr std::size_t kMaxInboxPerStep = 16;

    explicit WatchApp(const Services& s);
    WatchApp(const WatchApp&) = delete;
    WatchApp& operator=(const WatchApp&) = delete;

    bool init(TickMs now);

    bool postKey(const KeyEvent& e);
    bool postHr(const HrSample& s) override;
    bool postGps(const GpsSample& s) override;

    void step(TickMs now);
    bool needsRender() const { return m_dirty; }
    void render(ICanvas& c);

    const UiModel& model() const { return m_model; }
    ActivityState activityState() const { return m_session.state(); }
    const char* topScreenName() const;
    const char* overlayTitle() const { return m_overlay.title(); }
    uint16_t inboxDropped() const;
    uint16_t busDropped() const { return m_bus.dropped(); }
    void setTrace(IEventTrace* t) { m_bus.setTrace(t); }
    const ActivitySession& session() const { return m_session; }
    const Navigator& navigator() const { return m_nav; }

private:
    struct InboxMsg {
        enum class Kind : uint8_t { Key, Hr, Gps } kind;
        union {
            KeyEvent key;
            HrSample hr;
            GpsSample gps;
        };
    };

    bool post(const InboxMsg& m);
    void drainInbox();
    void onEvent(const Event& e) override;
    void buildModel();

    Services m_svc;

    EventBus m_bus;
    SoftTimerPool m_timers;
    RingBuffer<InboxMsg, kInboxCapacity> m_inbox;
    uint16_t m_inboxDropped = 0;

    GestureRecognizer m_gestures;
    InputRouter m_router;
    SettingsStore m_settings;
    ActivitySession m_session;
    HrZoneMonitor m_zones;

    Navigator m_nav;
    MenuScreen m_activityMenu{"ACTIVITY"};
    MenuScreen m_pauseMenu{"PAUSED", false};
    WatchFaceScreen m_watchFace;
    ActivityDataScreen m_dataScreen;
    SettingsScreen m_settingsScreen;

    ActivityCommand m_cmdRun;
    PushScreenCommand m_cmdSettings;
    ActivityCommand m_cmdResume;
    ActivityCommand m_cmdSave;
    ActivityCommand m_cmdDiscard;

    KeyLockHandler m_keyLock;
    OverlayManager m_overlay;
    BacklightHandler m_backlight;
    ActivityFlowCoordinator m_flow;

    UiModel m_model;
    TickMs m_now = 0;
    TickMs m_nextSecond = 0;
    bool m_dirty = true;
    bool m_initialized = false;

    Subscription m_sub;
};
}
