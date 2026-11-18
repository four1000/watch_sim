#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "ap/input/gesture_recognizer.h"
#include "ap/input/input_router.h"

using namespace ap;

namespace {
KeyEvent down(KeyId k, TickMs t) { return KeyEvent{k, KeyEdge::Down, t}; }
KeyEvent up(KeyId k, TickMs t) { return KeyEvent{k, KeyEdge::Up, t}; }

std::vector<GestureEvent> drain(GestureRecognizer& g) {
    std::vector<GestureEvent> out;
    while (auto e = g.next()) out.push_back(*e);
    return out;
}
}  // namespace

// ---- GestureRecognizer -----------------------------------------------------------

TEST(Gesture, ShortPressReportsPressTime) {
    GestureRecognizer g;
    g.onKeyEvent(down(KeyId::Start, 1000));
    g.onKeyEvent(up(KeyId::Start, 1100));
    const auto out = drain(g);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].gesture, Gesture::Short);
    EXPECT_EQ(out[0].timestampMs, 1000u);
}

TEST(Gesture, LongPressFiresWhileHeldAndReleaseIsSilent) {
    GestureRecognizer g;
    g.onKeyEvent(down(KeyId::Up, 0));
    g.poll(799);
    EXPECT_EQ(g.pending(), 0u);
    g.poll(800);
    auto out = drain(g);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].gesture, Gesture::Long);
    g.onKeyEvent(up(KeyId::Up, 2000));
    EXPECT_EQ(g.pending(), 0u);
}

TEST(Gesture, LateUpEdgeStillCountsAsLong) {
    GestureRecognizer g;  // no poll() between edges: the Up edge itself must judge time
    g.onKeyEvent(down(KeyId::Back, 0));
    g.onKeyEvent(up(KeyId::Back, 900));
    const auto out = drain(g);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].gesture, Gesture::Long);
}

TEST(Gesture, DoubleOnlyWhenEnabled) {
    GestureRecognizer g;
    g.setDoubleEnabled(KeyId::Light, true);
    g.onKeyEvent(down(KeyId::Light, 0));
    g.onKeyEvent(up(KeyId::Light, 80));
    g.onKeyEvent(down(KeyId::Light, 200));
    g.onKeyEvent(up(KeyId::Light, 260));
    auto out = drain(g);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].gesture, Gesture::Double);

    // Same edges on a key without double detection -> two shorts.
    g.onKeyEvent(down(KeyId::Up, 1000));
    g.onKeyEvent(up(KeyId::Up, 1080));
    g.onKeyEvent(down(KeyId::Up, 1200));
    g.onKeyEvent(up(KeyId::Up, 1260));
    out = drain(g);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[1].gesture, Gesture::Short);
}

TEST(Gesture, ShortOnDoubleKeyWaitsForWindow) {
    GestureRecognizer g;
    g.setDoubleEnabled(KeyId::Light, true);
    g.onKeyEvent(down(KeyId::Light, 0));
    g.onKeyEvent(up(KeyId::Light, 50));
    g.poll(349);
    EXPECT_EQ(g.pending(), 0u) << "latency is the price of double-click detection";
    g.poll(350);
    const auto out = drain(g);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].gesture, Gesture::Short);
    EXPECT_EQ(out[0].timestampMs, 0u);
}

TEST(Gesture, KeysAreIndependent) {
    GestureRecognizer g;
    g.onKeyEvent(down(KeyId::Up, 0));
    g.onKeyEvent(down(KeyId::Down, 100));
    g.onKeyEvent(up(KeyId::Down, 200));
    g.poll(800);
    const auto out = drain(g);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].key, KeyId::Down);
    EXPECT_EQ(out[0].gesture, Gesture::Short);
    EXPECT_EQ(out[1].key, KeyId::Up);
    EXPECT_EQ(out[1].gesture, Gesture::Long);
}

TEST(Gesture, WorksAcrossTickWrap) {
    GestureRecognizer g;
    g.onKeyEvent(down(KeyId::Start, 0xFFFFFF00u));
    g.poll(0x00000200u);  // 0x300 ms later = 768 ms < 800
    EXPECT_EQ(g.pending(), 0u);
    g.poll(0x00000220u);  // 800 ms
    EXPECT_EQ(g.pending(), 1u);
}

TEST(Gesture, InvalidInputRejectedAndIgnoredEdgesAccepted) {
    GestureRecognizer g;
    EXPECT_FALSE(g.onKeyEvent(KeyEvent{KeyId::Count, KeyEdge::Down, 0}));
    EXPECT_FALSE(g.onKeyEvent(KeyEvent{KeyId::Up, static_cast<KeyEdge>(7), 0}));
    EXPECT_TRUE(g.onKeyEvent(up(KeyId::Up, 0))) << "Up while idle: ignored";
    EXPECT_EQ(g.pending(), 0u);
}

TEST(Gesture, OverflowSaturatesDropCounter) {
    GestureRecognizer g;
    for (TickMs t = 0; t < 20 * 100; t += 100) {
        g.onKeyEvent(down(KeyId::Up, t));
        g.onKeyEvent(up(KeyId::Up, t + 10));
    }
    EXPECT_EQ(g.pending(), GestureRecognizer::kQueueCapacity);
    EXPECT_EQ(g.dropped(), 20u - GestureRecognizer::kQueueCapacity);
    g.reset();
    EXPECT_EQ(g.pending(), 0u);
    EXPECT_EQ(g.dropped(), 0u);
}

// ---- InputRouter (Chain of Responsibility) -------------------------------------------

namespace {
struct Handler final : IInputHandler {
    explicit Handler(bool consume) : consume(consume) {}
    bool handle(const GestureEvent&) override { ++calls; return consume; }
    const char* handlerName() const override { return "test"; }
    bool consume;
    int calls = 0;
};
const GestureEvent kStart{KeyId::Start, Gesture::Short, 0};
}  // namespace

static_assert(std::is_abstract_v<IInputHandler>);
static_assert(!std::is_copy_constructible_v<IInputHandler>);
static_assert(std::is_abstract_v<ICommand>);

TEST(InputRouter, FirstConsumerStopsTheChain) {
    InputRouter r;
    Handler a(false), b(true), c(true);
    r.append(a);
    r.append(b);
    r.append(c);
    EXPECT_EQ(r.route(kStart), &b);
    EXPECT_EQ(a.calls, 1);
    EXPECT_EQ(b.calls, 1);
    EXPECT_EQ(c.calls, 0);
}

TEST(InputRouter, UnhandledIsCounted) {
    InputRouter r;
    Handler a(false);
    r.append(a);
    EXPECT_EQ(r.route(kStart), nullptr);
    EXPECT_EQ(r.unhandled(), 1u);
}

TEST(InputRouter, RejectsDuplicatesOverflowAndInvalidKeys) {
    InputRouter r;
    Handler h[InputRouter::kMaxHandlers + 1] = {Handler(false), Handler(false), Handler(false),
                                                Handler(false), Handler(false), Handler(false), Handler(false)};
    EXPECT_TRUE(r.append(h[0]));
    EXPECT_FALSE(r.append(h[0]));
    for (std::size_t i = 1; i < InputRouter::kMaxHandlers; ++i) EXPECT_TRUE(r.append(h[i]));
    EXPECT_FALSE(r.append(h[InputRouter::kMaxHandlers]));
    EXPECT_EQ(r.route(GestureEvent{KeyId::Count, Gesture::Short, 0}), nullptr);
    EXPECT_EQ(h[0].calls, 0);
}
