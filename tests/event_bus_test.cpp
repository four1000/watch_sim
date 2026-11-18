#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "ap/event/event_bus.h"

using namespace ap;

namespace {
struct Listener : IEventListener {
    void onEvent(const Event& e) override {
        types.push_back(e.type);
        if (onHook) onHook(e);
    }
    std::vector<EventType> types;
    void (*onHook)(const Event&) = nullptr;
};

Event tick(TickMs t = 0) { return Event::makeSecondTick(t); }
Event settings(TickMs t = 0) { return Event::makeSettingsChanged(t); }
}  // namespace

static_assert(!std::is_copy_constructible_v<Subscription>);
static_assert(std::is_nothrow_move_constructible_v<Subscription>);
static_assert(!std::is_copy_constructible_v<EventBus>);

TEST(EventBus, PublishIsDeferredUntilDispatch) {
    EventBus bus;
    Listener l;
    auto sub = bus.subscribe(l, topic(EventType::SecondTick));
    ASSERT_TRUE(sub.active());
    bus.publish(tick());
    EXPECT_TRUE(l.types.empty());
    EXPECT_EQ(bus.dispatch(), 1u);
    EXPECT_EQ(l.types.size(), 1u);
}

TEST(EventBus, TopicMaskFilters) {
    EventBus bus;
    Listener ticks, all;
    auto s1 = bus.subscribe(ticks, topic(EventType::SecondTick));
    auto s2 = bus.subscribe(all, topics(EventType::SecondTick, EventType::SettingsChanged));
    bus.publish(tick());
    bus.publish(settings());
    bus.dispatch();
    EXPECT_EQ(ticks.types.size(), 1u);
    EXPECT_EQ(all.types.size(), 2u);
}

TEST(EventBus, SubscriptionTokenUnsubscribesOnDestruction) {
    EventBus bus;
    Listener l;
    {
        auto sub = bus.subscribe(l, topic(EventType::SecondTick));
        EXPECT_EQ(bus.subscribers(), 1u);
    }
    EXPECT_EQ(bus.subscribers(), 0u);
    bus.publish(tick());
    bus.dispatch();
    EXPECT_TRUE(l.types.empty());
}

TEST(EventBus, MovedTokenKeepsSubscription) {
    EventBus bus;
    Listener l;
    Subscription outer;
    {
        auto inner = bus.subscribe(l, topic(EventType::SecondTick));
        outer = std::move(inner);
    }
    EXPECT_TRUE(outer.active());
    bus.publish(tick());
    bus.dispatch();
    EXPECT_EQ(l.types.size(), 1u);
}

TEST(EventBus, QueueOverflowIsCountedNotGrown) {
    EventBus bus;
    for (std::size_t i = 0; i < EventBus::kQueueCapacity; ++i) EXPECT_TRUE(bus.publish(tick()));
    EXPECT_FALSE(bus.publish(tick()));
    EXPECT_EQ(bus.dropped(), 1u);
}

TEST(EventBus, ZeroMaskOrFullTableGivesInactiveToken) {
    EventBus bus;
    Listener l;
    EXPECT_FALSE(bus.subscribe(l, 0).active());
    std::vector<Subscription> subs;
    for (std::size_t i = 0; i < EventBus::kMaxSubscribers; ++i) subs.push_back(bus.subscribe(l, topic(EventType::SecondTick)));
    EXPECT_FALSE(bus.subscribe(l, topic(EventType::SecondTick)).active());
}

// Listener that unsubscribes *another* listener while an event is being delivered.
TEST(EventBus, UnsubscribeDuringDispatchIsSafe) {
    EventBus bus;
    static Subscription* victim = nullptr;
    Listener killer, target;
    killer.onHook = [](const Event&) { victim->reset(); };
    auto s1 = bus.subscribe(killer, topic(EventType::SecondTick));
    auto s2 = bus.subscribe(target, topic(EventType::SecondTick));
    victim = &s2;
    bus.publish(tick());
    bus.dispatch();
    EXPECT_TRUE(target.types.empty()) << "tombstoned slot is skipped";
    victim = nullptr;
}

TEST(EventBus, SubscribeDuringDispatchStartsWithNextEvent) {
    EventBus bus;
    static EventBus* theBus = nullptr;
    static Listener* late = nullptr;
    static Subscription lateSub;
    Listener first, lateListener;
    theBus = &bus;
    late = &lateListener;
    first.onHook = [](const Event&) {
        if (!lateSub.active()) lateSub = theBus->subscribe(*late, topic(EventType::SecondTick));
    };
    auto s = bus.subscribe(first, topic(EventType::SecondTick));
    bus.publish(tick(1));
    bus.publish(tick(2));
    bus.dispatch();
    EXPECT_EQ(lateListener.types.size(), 1u) << "receives only the second event";
    lateSub.reset();
}

TEST(EventBus, EventsPublishedDuringDispatchAreDeliveredInOrder) {
    EventBus bus;
    static EventBus* theBus = nullptr;
    theBus = &bus;
    Listener chain;
    chain.onHook = [](const Event& e) {
        if (e.type == EventType::SecondTick) theBus->publish(Event::makeSettingsChanged(0));
    };
    auto s = bus.subscribe(chain, topics(EventType::SecondTick, EventType::SettingsChanged));
    bus.publish(tick());
    EXPECT_EQ(bus.dispatch(), 2u);
    ASSERT_EQ(chain.types.size(), 2u);
    EXPECT_EQ(chain.types[0], EventType::SecondTick);
    EXPECT_EQ(chain.types[1], EventType::SettingsChanged);
}

TEST(EventBus, BudgetBoundsPingPong) {
    EventBus bus;
    static EventBus* theBus = nullptr;
    theBus = &bus;
    Listener echo;
    echo.onHook = [](const Event&) { theBus->publish(Event::makeSecondTick(0)); };
    auto s = bus.subscribe(echo, topic(EventType::SecondTick));
    bus.publish(tick());
    EXPECT_EQ(bus.dispatch(5), 5u) << "an event loop cannot starve the main loop";
    EXPECT_EQ(bus.pending(), 1u);
}

TEST(Event, DescribeFormatsPayload) {
    char buf[64];
    describe(Event::makeHr(HrSample{0, 142, true}), buf, sizeof buf);
    EXPECT_STREQ(buf, "HrSample 142 bpm");
    describe(Event::makeActivity(0, ActivityState::Paused, ActivityState::Idle, ActivityOutcome::Saved), buf, sizeof buf);
    EXPECT_STREQ(buf, "ActivityState Paused -> Idle (saved)");
}
