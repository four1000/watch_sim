// End-to-end tests of the whole AP layer: real WatchApp, fake services.
// Input goes in as raw key edges, output is checked on the screen stack,
// the overlay, the recorder and the draw calls.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

#include "ap/app/watch_app.h"
#include "fakes.h"

using namespace ap;

// ---- Global allocation counter ------------------------------------------------------
// Replaces operator new for this test binary. Only *counts* - allocation
// still works, so GoogleTest itself is unaffected.
namespace {
std::atomic<long> g_allocs{0};
}
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {

struct App : ::testing::Test {
    fakes::FakeGps gps;
    fakes::FakeHr hr;
    fakes::FakeRecorder recorder;
    fakes::FakeVibrator vibrator;
    fakes::FakeBacklight backlight;
    fakes::FakeClock clock;
    fakes::NullMutex mutex;
    WatchApp app{WatchApp::Services{gps, hr, recorder, vibrator, backlight, clock, mutex}};
    fakes::FakeCanvas canvas;
    TickMs now = 1000;

    void SetUp() override { ASSERT_TRUE(app.init(now)); }

    void advance(uint32_t ms) {
        for (uint32_t i = 0; i < ms; i += 20) {
            now += 20;
            app.step(now);
        }
    }
    void press(KeyId k, uint32_t holdMs = 80) {
        app.postKey(KeyEvent{k, KeyEdge::Down, now});
        advance(holdMs);
        app.postKey(KeyEvent{k, KeyEdge::Up, now});
        advance(400);  // longer than the double-click window
    }
    void hold(KeyId k) { press(k, 1000); }
    std::string top() const { return app.topScreenName(); }
};

}  // namespace

TEST_F(App, BootsToWatchFaceAndStartsHr) {
    EXPECT_EQ(top(), "WatchFace");
    EXPECT_TRUE(hr.running);
    EXPECT_FALSE(gps.running);
    app.render(canvas);
    EXPECT_TRUE(canvas.shows("08:42"));
}

TEST_F(App, FullRunFlowThroughKeys) {
    press(KeyId::Start);  // watch face -> menu
    EXPECT_EQ(top(), "ACTIVITY");
    press(KeyId::Start);  // "Run"
    EXPECT_EQ(top(), "ActivityData");
    EXPECT_EQ(app.activityState(), ActivityState::Ready);
    EXPECT_TRUE(gps.running);

    press(KeyId::Start);  // start timer
    EXPECT_EQ(app.activityState(), ActivityState::Running);
    for (int i = 0; i < 10; ++i) {
        app.postGps(GpsSample{now, static_cast<uint32_t>(i * 30), 5, true});
        app.postHr(HrSample{now, 125, true});
        advance(1000);
    }
    press(KeyId::Back);  // lap
    EXPECT_STREQ(app.overlayTitle(), "LAP 1");

    press(KeyId::Start);  // pause -> coordinator pushes pause menu
    EXPECT_EQ(app.activityState(), ActivityState::Paused);
    EXPECT_EQ(top(), "PAUSED");
    press(KeyId::Down);   // Save
    press(KeyId::Start);
    EXPECT_EQ(app.activityState(), ActivityState::Idle);
    EXPECT_EQ(top(), "WatchFace") << "flow coordinator returns home";
    EXPECT_FALSE(gps.running);
    EXPECT_EQ(recorder.finishes, 1);
    EXPECT_EQ(recorder.summary.distanceDm, 270u);
    EXPECT_STREQ(app.overlayTitle(), "SAVED");
}

TEST_F(App, ClockHiddenInsideRunAndSettings) {
    auto clockShown = [&] {
        app.render(canvas);
        return canvas.shows("08:42");
    };
    EXPECT_TRUE(clockShown()) << "watch face";
    press(KeyId::Start);
    EXPECT_EQ(top(), "ACTIVITY");
    EXPECT_TRUE(clockShown()) << "activity menu still shows the time";

    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_EQ(top(), "Settings");
    EXPECT_FALSE(clockShown()) << "settings";
    press(KeyId::Back);
    press(KeyId::Up);

    press(KeyId::Start);  // Run
    EXPECT_EQ(top(), "ActivityData");
    EXPECT_FALSE(clockShown()) << "activity data (Ready)";
    press(KeyId::Start);
    EXPECT_FALSE(clockShown()) << "activity data (Running)";
    press(KeyId::Start);
    EXPECT_EQ(top(), "PAUSED");
    EXPECT_FALSE(clockShown()) << "pause menu is inside the activity";
}

TEST_F(App, ResumeFromPauseMenuReturnsToDataScreen) {
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);  // pause
    ASSERT_EQ(top(), "PAUSED");
    press(KeyId::Start);  // "Resume"
    EXPECT_EQ(app.activityState(), ActivityState::Running);
    EXPECT_EQ(top(), "ActivityData");
}

TEST_F(App, BackFromReadyCancelsActivity) {
    press(KeyId::Start);
    press(KeyId::Start);
    ASSERT_EQ(app.activityState(), ActivityState::Ready);
    press(KeyId::Back);
    EXPECT_EQ(app.activityState(), ActivityState::Idle);
    EXPECT_EQ(top(), "WatchFace");
}

TEST_F(App, GpsFailureShowsModalAlertAndStaysInMenu) {
    gps.startOk = false;
    press(KeyId::Start);
    press(KeyId::Start);
    EXPECT_EQ(top(), "ACTIVITY");
    EXPECT_STREQ(app.overlayTitle(), "START FAILED");
    press(KeyId::Down);  // any key dismisses the modal overlay...
    EXPECT_STREQ(app.overlayTitle(), "");
    press(KeyId::Down);  // ...and only then reaches the menu
    EXPECT_EQ(top(), "ACTIVITY");
}

TEST_F(App, HrAlertDuringRunIsModalAndVibrates) {
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);
    for (int i = 0; i < 5; ++i) {
        app.postHr(HrSample{now, 180, true});  // Z5, target Z2
        advance(1000);
    }
    EXPECT_STREQ(app.overlayTitle(), "HR HIGH");
    ASSERT_FALSE(vibrator.patterns.empty());
    EXPECT_EQ(vibrator.patterns.back(), VibePattern::Long);
    press(KeyId::Back);  // dismiss: must NOT take a lap
    EXPECT_EQ(app.session().metrics().lapCount(), 0u);
}

TEST_F(App, KeyLockSwallowsEverythingButDoubleLight) {
    // double LIGHT
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Down, now});
    advance(60);
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Up, now});
    advance(100);
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Down, now});
    advance(60);
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Up, now});
    advance(400);
    app.render(canvas);  // the model is rebuilt on render
    EXPECT_TRUE(app.model().keyLocked);
    press(KeyId::Start);
    EXPECT_EQ(top(), "WatchFace") << "locked";
}

TEST_F(App, LightTogglesBacklightWithTimeout) {
    press(KeyId::Light);
    EXPECT_TRUE(backlight.on);
    advance(BacklightHandler::kTimeoutMs);
    EXPECT_FALSE(backlight.on);
}

TEST_F(App, BacklightSwitchesToWhiteBackgroundWithDarkText) {
    app.render(canvas);
    EXPECT_EQ(canvas.background, Color::Black) << "backlight off: dark theme";
    EXPECT_EQ(canvas.colorOf("08:42"), Color::White);

    press(KeyId::Light);
    ASSERT_TRUE(backlight.on);
    ASSERT_TRUE(app.needsRender());
    app.render(canvas);
    EXPECT_EQ(canvas.background, Color::White) << "backlight on: light theme";
    EXPECT_EQ(canvas.colorOf("08:42"), Color::Black);

    // The timeout turns the light off; that alone must request a redraw.
    while (backlight.on) {
        now += 20;
        app.step(now);
        if (backlight.on && app.needsRender()) app.render(canvas);
    }
    EXPECT_TRUE(app.needsRender());
    app.render(canvas);
    EXPECT_EQ(canvas.background, Color::Black);
}

TEST_F(App, SettingsReachableFromMenuAndBackReturns) {
    press(KeyId::Start);
    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_EQ(top(), "Settings");
    press(KeyId::Back);
    EXPECT_EQ(top(), "ACTIVITY");
    press(KeyId::Back);
    EXPECT_EQ(top(), "WatchFace");
}

TEST_F(App, InboxOverflowIsCountedNotBlocking) {
    for (std::size_t i = 0; i < WatchApp::kInboxCapacity + 5; ++i) app.postHr(HrSample{now, 100, true});
    EXPECT_EQ(app.inboxDropped(), 5u);
    EXPECT_EQ(mutex.locks, mutex.unlocks) << "lock released on every path";
}

TEST_F(App, RendersOnlyWhenDirty) {
    app.render(canvas);
    EXPECT_FALSE(app.needsRender());
    now += 10;
    app.step(now);
    EXPECT_FALSE(app.needsRender()) << "nothing happened";
    advance(1000);
    EXPECT_TRUE(app.needsRender()) << "1 s tick";
}

// The key embedded-design claim: after init(), the AP layer never allocates.
// The fakes above use std::vector / std::string, so this test runs the same
// scenario with allocation-free fakes and counts every operator new.
namespace {
struct QuietRecorder final : IActivityRecorder {
    bool begin(TickMs) override { return true; }
    bool recordLap(const LapRecord&) override { ++laps; return true; }
    bool finish(const ActivitySummary&) override { ++finishes; return true; }
    void discard() override {}
    int laps = 0, finishes = 0;
};
struct QuietVibrator final : IVibrator {
    void vibrate(VibePattern) override { ++count; }
    int count = 0;
};
struct QuietCanvas final : ICanvas {
    int16_t width() const override { return 240; }
    int16_t height() const override { return 240; }
    void clear(Color) override {}
    void fillRect(int16_t, int16_t, int16_t, int16_t, Color) override {}
    void fillCircle(int16_t, int16_t, int16_t, Color) override {}
    void drawArc(int16_t, int16_t, int16_t, int16_t, int16_t, uint8_t, Color) override {}
    void drawLine(int16_t, int16_t, int16_t, int16_t, uint8_t, Color) override {}
    void drawText(int16_t, int16_t, std::string_view t, Font, Color, Align) override { chars += t.size(); }
    std::size_t chars = 0;
};
}  // namespace

TEST(AppMemory, ZeroHeapAllocationAfterInit) {
    fakes::FakeGps gps;
    fakes::FakeHr hr;
    QuietRecorder recorder;
    QuietVibrator vibrator;
    fakes::FakeBacklight backlight;
    fakes::FakeClock clock;
    fakes::NullMutex mutex;
    QuietCanvas canvas;
    WatchApp app{WatchApp::Services{gps, hr, recorder, vibrator, backlight, clock, mutex}};
    TickMs now = 0;
    ASSERT_TRUE(app.init(now));

    auto advance = [&](uint32_t ms) {
        for (uint32_t i = 0; i < ms; i += 20) {
            now += 20;
            app.step(now);
            if (app.needsRender()) app.render(canvas);
        }
    };
    auto press = [&](KeyId k) {
        app.postKey(KeyEvent{k, KeyEdge::Down, now});
        advance(80);
        app.postKey(KeyEvent{k, KeyEdge::Up, now});
        advance(400);
    };

    const long before = g_allocs.load();
    // --- measured region: no gtest macros inside (they may allocate) ---
    press(KeyId::Start);  // menu
    press(KeyId::Start);  // Run -> Ready
    press(KeyId::Start);  // Running
    for (int i = 0; i < 120; ++i) {
        app.postGps(GpsSample{now, static_cast<uint32_t>(i * 30), 5, true});
        app.postHr(HrSample{now, static_cast<uint8_t>(100 + i / 2), true});  // walks through every zone -> alerts
        advance(1000);
        if (i % 30 == 29) press(KeyId::Back);  // laps / dismiss alerts
    }
    press(KeyId::Down);
    press(KeyId::Start);  // pause
    press(KeyId::Down);
    press(KeyId::Start);  // save
    const long after = g_allocs.load();
    // --- end of measured region ---

    EXPECT_EQ(after - before, 0) << "operator new called inside the AP main loop";
    EXPECT_EQ(app.activityState(), ActivityState::Idle);
    EXPECT_EQ(recorder.finishes, 1);
    EXPECT_GT(vibrator.count, 0);
    EXPECT_GT(canvas.chars, 0u);
}

// Guard against a silently broken counter (e.g. operator new not replaced):
// the zero-allocation test above would then pass for the wrong reason.
TEST(AppMemory, AllocationCounterWorks) {
    const long before = g_allocs.load();
    auto p = std::make_unique<int>(42);
    EXPECT_GT(g_allocs.load() - before, 0);
    EXPECT_EQ(*p, 42);
}
