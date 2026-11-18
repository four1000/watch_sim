#include <gtest/gtest.h>

#include <vector>

#include "ap/app/settings.h"
#include "ap/health/hr_zone_monitor.h"

using namespace ap;

TEST(HrZones, BoundariesForMax190) {
    EXPECT_EQ(hrZoneLowerBound(1, 190), 95);
    EXPECT_EQ(hrZoneLowerBound(3, 190), 133);
    EXPECT_EQ(hrZoneLowerBound(5, 190), 171);
    EXPECT_EQ(hrZoneFor(94, 190), 0);
    EXPECT_EQ(hrZoneFor(95, 190), 1);
    EXPECT_EQ(hrZoneFor(150, 190), 3);
    EXPECT_EQ(hrZoneFor(220, 190), 5);
}

namespace {

struct MonitorFixture : ::testing::Test {
    struct Spy : IEventListener {
        void onEvent(const Event& e) override { events.push_back(e); }
        int count(EventType t) const {
            int n = 0;
            for (const auto& e : events) n += (e.type == t);
            return n;
        }
        std::vector<Event> events;
    };
    void SetUp() override {
        monitor.attach();
        spySub = bus.subscribe(spy, topics(EventType::HrZoneChanged, EventType::HrAlert));
    }
    void sample(TickMs t, uint8_t bpm, bool valid = true) {
        bus.publish(Event::makeHr(HrSample{t, bpm, valid}));
        bus.dispatch();
    }
    void running(bool on) {
        bus.publish(Event::makeActivity(0, on ? ActivityState::Ready : ActivityState::Running,
                                        on ? ActivityState::Running : ActivityState::Paused, ActivityOutcome::None));
        bus.dispatch();
    }

    EventBus bus;
    SettingsStore settings{bus};
    HrZoneMonitor monitor{bus, settings};
    Spy spy;
    Subscription spySub;
};
}

TEST_F(MonitorFixture, HysteresisPreventsFlicker) {
    sample(0, 140);
    EXPECT_EQ(monitor.zone(), 3);
    const int changes = spy.count(EventType::HrZoneChanged);
    sample(1000, 132);
    sample(2000, 134);
    sample(3000, 132);
    EXPECT_EQ(monitor.zone(), 3);
    EXPECT_EQ(spy.count(EventType::HrZoneChanged), changes);
    sample(4000, 130);
    EXPECT_EQ(monitor.zone(), 2);
}

TEST_F(MonitorFixture, NoAlertUnlessRunning) {
    for (TickMs t = 0; t < 10000; t += 1000) sample(t, 175);
    EXPECT_EQ(spy.count(EventType::HrAlert), 0);
}

TEST_F(MonitorFixture, AlertAfterDebounceThenRateLimited) {
    running(true);
    sample(0, 175);
    sample(2999, 175);
    EXPECT_EQ(spy.count(EventType::HrAlert), 0) << "debounce";
    sample(3000, 175);
    ASSERT_EQ(spy.count(EventType::HrAlert), 1);
    EXPECT_EQ(spy.events.back().alert.kind, HrAlertKind::AboveTarget);
    sample(10000, 175);
    EXPECT_EQ(spy.count(EventType::HrAlert), 1) << "no re-alert inside kRealertMs";
    sample(3000 + HrZoneMonitor::kRealertMs, 175);
    EXPECT_EQ(spy.count(EventType::HrAlert), 2);
}

TEST_F(MonitorFixture, BackInTargetResetsDebounce) {
    running(true);
    sample(0, 175);
    sample(2000, 120);
    sample(2500, 175);
    sample(5000, 175);
    EXPECT_EQ(spy.count(EventType::HrAlert), 0);
}

TEST_F(MonitorFixture, SensorLossClearsZoneAndAlert) {
    running(true);
    sample(0, 175);
    sample(1000, 0, false);
    EXPECT_FALSE(monitor.valid());
    EXPECT_EQ(monitor.zone(), 0);
    sample(2000, 175);
    sample(4000, 175);
    EXPECT_EQ(spy.count(EventType::HrAlert), 0) << "debounce restarted after sensor loss";
}

TEST_F(MonitorFixture, SettingsChangeRecomputesZone) {
    sample(0, 150);
    EXPECT_EQ(monitor.zone(), 3);
    EXPECT_TRUE(settings.setMaxHr(220, 0));
    bus.dispatch();
    EXPECT_EQ(monitor.zone(), 2);
}

TEST(Settings, RejectsOutOfRangeAndPublishesOnlyRealChanges) {
    EventBus bus;
    SettingsStore s(bus);
    EXPECT_FALSE(s.setMaxHr(UserSettings::kMinMaxHr - 1, 0));
    EXPECT_FALSE(s.setMaxHr(UserSettings::kMaxMaxHr + 1, 0));
    EXPECT_FALSE(s.setTargetZone(0, 0));
    EXPECT_FALSE(s.setTargetZone(6, 0));
    EXPECT_EQ(bus.pending(), 0u);
    EXPECT_TRUE(s.setTargetZone(2, 0)) << "same value: accepted, nothing published";
    EXPECT_EQ(bus.pending(), 0u);
    EXPECT_TRUE(s.setTargetZone(3, 0));
    EXPECT_EQ(bus.pending(), 1u);
}
