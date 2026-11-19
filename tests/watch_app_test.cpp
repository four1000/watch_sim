#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

#include "ap/app/watch_app.h"
#include "fakes.h"

using namespace ap;

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
    WatchApp app{WatchApp::Services{gps, hr, recorder, recorder, vibrator, backlight, clock, mutex}};
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
        advance(400);
    }
    void hold(KeyId k) { press(k, 1000); }
    std::string top() const { return app.topScreenName(); }
};
}

TEST_F(App, BootsToWatchFaceAndStartsHr) {
    EXPECT_EQ(top(), "WatchFace");
    EXPECT_TRUE(hr.running);
    EXPECT_FALSE(gps.running);
    app.render(canvas);
    EXPECT_TRUE(canvas.shows("08:42"));
}

TEST_F(App, FullRunFlowThroughKeys) {
    press(KeyId::Start);
    EXPECT_EQ(top(), "ACTIVITY");
    press(KeyId::Start);
    EXPECT_EQ(top(), "ActivityData");
    EXPECT_EQ(app.activityState(), ActivityState::Ready);
    EXPECT_TRUE(gps.running);

    press(KeyId::Start);
    EXPECT_EQ(app.activityState(), ActivityState::Running);
    for (int i = 0; i < 10; ++i) {
        app.postGps(GpsSample{now, static_cast<uint32_t>(i * 30), 5, true});
        app.postHr(HrSample{now, 125, true});
        advance(1000);
    }
    press(KeyId::Back);
    EXPECT_STREQ(app.overlayTitle(), "LAP 1");

    press(KeyId::Start);
    EXPECT_EQ(app.activityState(), ActivityState::Paused);
    EXPECT_EQ(top(), "PAUSED");
    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_EQ(app.activityState(), ActivityState::Idle);
    EXPECT_EQ(top(), "ActivityDetail") << "saved activity summary on top of home";
    EXPECT_EQ(app.navigator().depth(), 2u);
    EXPECT_FALSE(gps.running);
    EXPECT_EQ(recorder.finishes, 1);
    EXPECT_EQ(recorder.summary.distanceDm, 270u);
    EXPECT_STREQ(app.overlayTitle(), "SAVED");
    press(KeyId::Back);
    EXPECT_EQ(top(), "WatchFace");
}

TEST_F(App, SummaryShowsSavedActivityAndScrollsThroughLaps) {
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);
    for (int lap = 0; lap < 6; ++lap) {
        for (int s = 0; s < 3; ++s) {
            app.postGps(GpsSample{now, static_cast<uint32_t>((lap * 3 + s) * 30), 5, true});
            advance(1000);
        }
        press(KeyId::Back);
    }
    press(KeyId::Start);
    press(KeyId::Down);
    press(KeyId::Start);
    ASSERT_EQ(top(), "ActivityDetail");
    advance(3000);

    app.render(canvas);
    EXPECT_TRUE(canvas.shows("18 NOV 08:42"));
    EXPECT_TRUE(canvas.shows("7 LAPS")) << "6 manual laps + the final lap closed on save";

    press(KeyId::Down);
    app.render(canvas);
    EXPECT_TRUE(canvas.showsExactly("PACE"));
    EXPECT_TRUE(canvas.showsExactly("1"));
    EXPECT_TRUE(canvas.showsExactly("4"));
    EXPECT_FALSE(canvas.showsExactly("5")) << "four rows visible";
    for (int i = 0; i < 10; ++i) press(KeyId::Down);
    app.render(canvas);
    EXPECT_FALSE(canvas.showsExactly("3"));
    EXPECT_TRUE(canvas.showsExactly("4"));
    EXPECT_TRUE(canvas.showsExactly("7")) << "stops with the last lap on the last row";

    press(KeyId::Back);
    EXPECT_EQ(top(), "WatchFace");
}

TEST_F(App, HistoryListsSavedActivitiesNewestFirst) {
    press(KeyId::Start);
    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_EQ(top(), "History");
    app.render(canvas);
    EXPECT_TRUE(canvas.shows("NO ACTIVITIES"));
    press(KeyId::Start);
    EXPECT_EQ(top(), "History") << "nothing to open";

    recorder.addToHistory(ActivityRecord{LocalTime{2026, 11, 16, 1, 7, 5, 0}, ActivitySummary{1800000, 50000, 0, 140, 160}},
                          {});
    recorder.addToHistory(ActivityRecord{LocalTime{2026, 11, 17, 2, 18, 30, 0}, ActivitySummary{600000, 21000, 0, 0, 0}},
                          {});
    app.render(canvas);
    EXPECT_TRUE(canvas.shows("17 NOV 18:30")) << "newest selected";
    EXPECT_TRUE(canvas.shows("2.10 km"));
    EXPECT_TRUE(canvas.shows("16 NOV 07:05")) << "older one below";

    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_EQ(top(), "ActivityDetail");
    app.render(canvas);
    EXPECT_TRUE(canvas.shows("16 NOV 07:05"));
    EXPECT_TRUE(canvas.shows("5.00 km"));
    EXPECT_TRUE(canvas.shows("6:00 /km"));
    press(KeyId::Back);
    EXPECT_EQ(top(), "History");
    press(KeyId::Back);
    EXPECT_EQ(top(), "ACTIVITY");
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
    EXPECT_EQ(top(), "History");
    EXPECT_FALSE(clockShown()) << "history";
    press(KeyId::Back);
    press(KeyId::Down);
    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_EQ(top(), "Settings");
    EXPECT_FALSE(clockShown()) << "settings";
    press(KeyId::Back);

    press(KeyId::Start);
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
    press(KeyId::Start);
    ASSERT_EQ(top(), "PAUSED");
    press(KeyId::Start);
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
    press(KeyId::Down);
    EXPECT_STREQ(app.overlayTitle(), "");
    press(KeyId::Down);
    EXPECT_EQ(top(), "ACTIVITY");
}

TEST_F(App, HrAlertDuringRunIsModalAndVibrates) {
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);
    for (int i = 0; i < 100 && std::string(app.overlayTitle()) != "SLOW DOWN"; ++i) {
        if (i % 10 == 0) app.postHr(HrSample{now, 180, true});
        advance(100);
    }
    EXPECT_STREQ(app.overlayTitle(), "SLOW DOWN");
    ASSERT_FALSE(vibrator.patterns.empty());
    EXPECT_EQ(vibrator.patterns.back(), VibePattern::Long);
    press(KeyId::Back);
    EXPECT_EQ(app.session().metrics().lapCount(), 0u);
}

TEST_F(App, HrBelowTargetAsksRunnerToSpeedUp) {
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);
    for (int i = 0; i < 100 && std::string(app.overlayTitle()) != "SPEED UP"; ++i) {
        if (i % 10 == 0) app.postHr(HrSample{now, 80, true});
        advance(100);
    }
    EXPECT_STREQ(app.overlayTitle(), "SPEED UP");
}

TEST_F(App, KeyLockSwallowsEverythingButDoubleLight) {
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Down, now});
    advance(60);
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Up, now});
    advance(100);
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Down, now});
    advance(60);
    app.postKey(KeyEvent{KeyId::Light, KeyEdge::Up, now});
    advance(400);
    app.render(canvas);
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

namespace {

struct QuietRecorder final : IActivityRecorder, IActivityHistory {
    static constexpr std::size_t kMaxLaps = 8;

    bool begin(TickMs) override { laps = 0; return true; }
    bool recordLap(const LapRecord& lap) override {
        if (laps < kMaxLaps) lapStore[laps++] = lap;
        return true;
    }
    bool finish(const ActivitySummary& s) override {
        ++finishes;
        saved = ActivityRecord{LocalTime{2026, 11, 18, 3, 8, 42, 0}, s};
        return true;
    }
    void discard() override {}

    std::size_t activityCount() const override { return finishes > 0 ? 1 : 0; }
    bool activityAt(std::size_t i, ActivityRecord& out) const override {
        if (i >= activityCount()) return false;
        out = saved;
        return true;
    }
    bool lapAt(std::size_t i, std::size_t lap, LapRecord& out) const override {
        if (i >= activityCount() || lap >= laps) return false;
        out = lapStore[lap];
        return true;
    }

    std::array<LapRecord, kMaxLaps> lapStore{};
    std::size_t laps = 0;
    int finishes = 0;
    ActivityRecord saved{};
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
}

TEST(AppMemory, ZeroHeapAllocationAfterInit) {
    fakes::FakeGps gps;
    fakes::FakeHr hr;
    QuietRecorder recorder;
    QuietVibrator vibrator;
    fakes::FakeBacklight backlight;
    fakes::FakeClock clock;
    fakes::NullMutex mutex;
    QuietCanvas canvas;
    WatchApp app{WatchApp::Services{gps, hr, recorder, recorder, vibrator, backlight, clock, mutex}};
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
    press(KeyId::Start);
    press(KeyId::Start);
    press(KeyId::Start);
    for (int i = 0; i < 120; ++i) {
        app.postGps(GpsSample{now, static_cast<uint32_t>(i * 30), 5, true});
        app.postHr(HrSample{now, static_cast<uint8_t>(100 + i / 2), true});
        advance(1000);
        if (i % 30 == 29) press(KeyId::Back);
    }
    press(KeyId::Down);
    press(KeyId::Start);
    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_STREQ(app.topScreenName(), "ActivityDetail");
    for (int i = 0; i < 6; ++i) press(KeyId::Down);
    press(KeyId::Back);
    press(KeyId::Start);
    press(KeyId::Down);
    press(KeyId::Start);
    EXPECT_STREQ(app.topScreenName(), "History");
    press(KeyId::Start);
    press(KeyId::Down);
    EXPECT_STREQ(app.topScreenName(), "ActivityDetail");
    const long after = g_allocs.load();

    EXPECT_EQ(after - before, 0) << "operator new called inside the AP main loop";
    EXPECT_EQ(app.activityState(), ActivityState::Idle);
    EXPECT_EQ(recorder.finishes, 1);
    EXPECT_GT(vibrator.count, 0);
    EXPECT_GT(canvas.chars, 0u);
}

TEST(AppMemory, AllocationCounterWorks) {
    const long before = g_allocs.load();
    auto p = std::make_unique<int>(42);
    EXPECT_GT(g_allocs.load() - before, 0);
    EXPECT_EQ(*p, 42);
}
