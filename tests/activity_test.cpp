#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "ap/activity/activity_metrics.h"
#include "ap/activity/activity_session.h"
#include "fakes.h"

using namespace ap;

namespace {
GpsSample fix(TickMs t, uint32_t odo, uint16_t acc = 5) { return GpsSample{t, odo, acc, true}; }
HrSample hr(TickMs t, uint8_t bpm) { return HrSample{t, bpm, true}; }
}  // namespace

// ---- ActivityMetrics (pure computation) ---------------------------------------------

TEST(Metrics, ElapsedExcludesPauses) {
    ActivityMetrics m;
    m.start(1000);
    m.pause(61000);   // 60 s
    m.resume(120000); // 59 s paused
    EXPECT_EQ(m.elapsedMs(150000), 90000u);
}

TEST(Metrics, DistanceFromOdometerDeltasAcrossWrap) {
    ActivityMetrics m;
    m.start(0);
    m.onGps(fix(0, 0xFFFFFF00u));  // baseline: the odometer does not start at 0
    m.onGps(fix(1000, 0xFFFFFF80u));  // +128 dm
    m.onGps(fix(2000, 0x00000010u));  // wrapped: +144 dm (still under the 15 m/s glitch limit)
    EXPECT_EQ(m.distanceDm(), 128u + 144u);
}

TEST(Metrics, DistanceWhilePausedIsNotCounted) {
    ActivityMetrics m;
    m.start(0);
    m.onGps(fix(0, 1000));
    m.onGps(fix(1000, 1030));
    m.pause(1000);
    m.onGps(fix(5000, 5000));  // walking to the car while paused
    m.resume(6000);
    m.onGps(fix(6000, 6000));  // new baseline
    m.onGps(fix(7000, 6030));
    EXPECT_EQ(m.distanceDm(), 60u);
}

TEST(Metrics, GlitchAndPoorAccuracyRejected) {
    ActivityMetrics m;
    m.start(0);
    m.onGps(fix(0, 0));
    m.onGps(fix(1000, 30, 200));    // accuracy 200 m: ignored
    m.onGps(fix(2000, 60));
    m.onGps(fix(3000, 60 + 5000));  // 500 m in 1 s: glitch, re-baseline
    m.onGps(fix(4000, 60 + 5000 + 30));
    EXPECT_EQ(m.distanceDm(), 90u);
}

TEST(Metrics, PaceCalculation) {
    EXPECT_EQ(ActivityMetrics::paceSecKm(300000, 10000), 300u);  // 5:00 /km
    EXPECT_EQ(ActivityMetrics::paceSecKm(1000, 10), 0u) << "standing still";
    EXPECT_EQ(ActivityMetrics::paceSecKm(0xFFFFFFFFu, 20), ActivityMetrics::kMaxPaceSecKm) << "clamped, no overflow";

    ActivityMetrics m;
    m.start(0);
    for (TickMs t = 0; t <= 5000; t += 1000) m.onGps(fix(t, t / 1000 * 33));  // 3.3 m/s
    EXPECT_EQ(m.currentPaceSecKm(), 303u);  // 5:03 /km
}

TEST(Metrics, LapsSplitTimeDistanceAndHr) {
    ActivityMetrics m;
    m.start(0);
    m.onGps(fix(0, 0));
    m.onHr(hr(0, 140));
    m.onGps(fix(60000, 2000));
    m.onHr(hr(60000, 150));
    const LapRecord l1 = m.lap(60000);
    EXPECT_EQ(l1.index, 1u);
    EXPECT_EQ(l1.durationMs, 60000u);
    EXPECT_EQ(l1.distanceDm, 2000u);
    EXPECT_EQ(l1.avgHr, 145u);
    m.onHr(hr(90000, 160));
    const LapRecord l2 = m.lap(90000);
    EXPECT_EQ(l2.durationMs, 30000u);
    EXPECT_EQ(l2.distanceDm, 0u);
    EXPECT_EQ(l2.avgHr, 160u);
    EXPECT_EQ(m.summary(90000).avgHr, 150u);
    EXPECT_EQ(m.summary(90000).maxHr, 160u);
}

TEST(Metrics, LapStorageFullStillCountsLaps) {
    ActivityMetrics m;
    m.start(0);
    for (TickMs i = 1; i <= ActivityMetrics::kMaxStoredLaps + 3; ++i) m.lap(i * 1000);
    EXPECT_EQ(m.lapCount(), ActivityMetrics::kMaxStoredLaps + 3);
    EXPECT_EQ(m.storedLaps(), ActivityMetrics::kMaxStoredLaps);
    EXPECT_EQ(m.storedLap(ActivityMetrics::kMaxStoredLaps), nullptr);
    ActivitySnapshot snap;
    m.fill((ActivityMetrics::kMaxStoredLaps + 3) * 1000, snap);
    EXPECT_EQ(snap.lastLap.index, ActivityMetrics::kMaxStoredLaps + 3) << "last lap is still correct";
}

// ---- ActivitySession (State pattern) --------------------------------------------------

static_assert(std::is_abstract_v<IActivityControl>);
static_assert(!std::is_copy_constructible_v<ActivitySession>);

namespace {
struct SessionFixture : ::testing::Test {
    struct Spy : IEventListener {
        void onEvent(const Event& e) override { events.push_back(e); }
        std::vector<Event> events;
    };

    void SetUp() override {
        session.attach();
        spySub = bus.subscribe(spy, topics(EventType::ActivityStateChanged, EventType::LapCompleted));
    }
    void deliver() { bus.dispatch(); }
    const Event& last() const { return spy.events.back(); }

    EventBus bus;
    fakes::FakeGps gps;
    fakes::FakeRecorder recorder;
    ActivitySession session{gps, recorder, bus};
    Spy spy;
    Subscription spySub;
};
}  // namespace

TEST_F(SessionFixture, HappyPathIdleReadyRunningPausedSaved) {
    EXPECT_TRUE(session.prepare(0));
    EXPECT_TRUE(gps.running);
    EXPECT_EQ(session.state(), ActivityState::Ready);
    EXPECT_TRUE(session.startPause(1000));
    EXPECT_EQ(recorder.begins, 1);
    EXPECT_EQ(session.state(), ActivityState::Running);
    EXPECT_TRUE(session.startPause(61000));
    EXPECT_EQ(session.state(), ActivityState::Paused);
    EXPECT_TRUE(session.save(70000));
    EXPECT_EQ(session.state(), ActivityState::Idle);
    EXPECT_FALSE(gps.running) << "entering Idle always stops GPS";
    EXPECT_EQ(recorder.finishes, 1);
    EXPECT_EQ(recorder.summary.elapsedMs, 60000u);
    ASSERT_EQ(recorder.laps.size(), 1u) << "final partial lap is closed on save";

    deliver();
    ASSERT_EQ(spy.events.size(), 4u);
    EXPECT_EQ(last().activity.from, ActivityState::Paused);
    EXPECT_EQ(last().activity.outcome, ActivityOutcome::Saved);
}

TEST_F(SessionFixture, RequestsInvalidForTheStateAreIgnored) {
    EXPECT_FALSE(session.lap(0));
    EXPECT_FALSE(session.save(0));
    EXPECT_FALSE(session.startPause(0)) << "cannot start from Idle without prepare";
    session.prepare(0);
    EXPECT_FALSE(session.prepare(0));
    EXPECT_FALSE(session.lap(0)) << "no lap before the timer starts";
    session.startPause(0);
    EXPECT_FALSE(session.save(0)) << "must pause before saving";
    EXPECT_FALSE(session.cancel(0));
    deliver();
    EXPECT_EQ(spy.events.size(), 2u) << "ignored requests publish nothing";
}

TEST_F(SessionFixture, GpsStartFailureStaysIdleAndReports) {
    gps.startOk = false;
    EXPECT_FALSE(session.prepare(0));
    EXPECT_EQ(session.state(), ActivityState::Idle);
    deliver();
    ASSERT_EQ(spy.events.size(), 1u);
    EXPECT_EQ(last().activity.from, last().activity.to);
    EXPECT_EQ(last().activity.outcome, ActivityOutcome::StartFailed);
}

TEST_F(SessionFixture, RecorderBeginFailureStaysReady) {
    recorder.beginOk = false;
    session.prepare(0);
    EXPECT_FALSE(session.startPause(0));
    EXPECT_EQ(session.state(), ActivityState::Ready);
}

TEST_F(SessionFixture, SaveFailureStaysPausedAndRetryDoesNotDuplicateLap) {
    session.prepare(0);
    session.startPause(0);
    session.startPause(10000);
    recorder.finishOk = false;
    EXPECT_FALSE(session.save(11000));
    EXPECT_EQ(session.state(), ActivityState::Paused);
    recorder.finishOk = true;
    EXPECT_TRUE(session.save(12000));
    EXPECT_EQ(recorder.laps.size(), 1u);
}

TEST_F(SessionFixture, CancelAndDiscard) {
    session.prepare(0);
    EXPECT_TRUE(session.cancel(10));
    EXPECT_EQ(session.state(), ActivityState::Idle);
    EXPECT_EQ(recorder.begins, 0);

    session.prepare(20);
    session.startPause(30);
    session.startPause(40);
    EXPECT_TRUE(session.discard(50));
    EXPECT_EQ(recorder.discards, 1);
    EXPECT_EQ(recorder.finishes, 0);
}

TEST_F(SessionFixture, SamplesOnlyCountWhileRunning) {
    session.prepare(0);
    bus.publish(Event::makeGps(fix(0, 0)));
    bus.publish(Event::makeGps(fix(1000, 500)));  // Ready: ignored by metrics
    deliver();
    session.startPause(1000);
    bus.publish(Event::makeGps(fix(1000, 500)));
    bus.publish(Event::makeGps(fix(2000, 530)));
    deliver();
    EXPECT_EQ(session.snapshot(2000).distanceDm, 30u);
    EXPECT_TRUE(session.snapshot(2000).gpsFix);
    EXPECT_FALSE(session.snapshot(2000 + ActivitySession::kGpsStaleMs + 1).gpsFix) << "stale fix";
}

TEST_F(SessionFixture, LapPublishesEventWithGestureTime) {
    session.prepare(0);
    session.startPause(0);
    EXPECT_TRUE(session.lap(5000));
    deliver();
    EXPECT_EQ(last().type, EventType::LapCompleted);
    EXPECT_EQ(last().lap.durationMs, 5000u);
    EXPECT_EQ(recorder.laps.size(), 1u);
}
