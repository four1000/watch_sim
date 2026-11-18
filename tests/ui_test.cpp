#include <gtest/gtest.h>

#include <type_traits>

#include "ap/ui/navigator.h"
#include "ap/ui/screens.h"
#include "ap/ui/themed_canvas.h"
#include "fakes.h"

using namespace ap;

namespace {

GestureEvent press(KeyId k, Gesture g = Gesture::Short, TickMs t = 0) { return GestureEvent{k, g, t}; }

class TestScreen final : public Screen {
public:
    explicit TestScreen(const char* n, bool consumeAll = false) : m_name(n), m_consumeAll(consumeAll) {}
    const char* name() const override { return m_name; }
    int enters = 0, exits = 0, gestures = 0;

protected:
    void onEnter(TickMs) override { ++enters; }
    void onExit(TickMs) override { ++exits; }
    bool onGesture(const GestureEvent&) override { ++gestures; return m_consumeAll; }
    void onRender(ICanvas& c, const UiModel&) override { c.drawText(0, 0, m_name, Font::Small, Color::White, Align::Left); }

private:
    const char* m_name;
    bool m_consumeAll;
};

struct CountingCommand final : ICommand {
    void execute(TickMs t) override { ++runs; at = t; }
    int runs = 0;
    TickMs at = 0;
};

struct FakeControl final : IActivityControl {
    bool prepare(TickMs) override { return ++prepares, true; }
    bool cancel(TickMs) override { return ++cancels, true; }
    bool startPause(TickMs) override { return ++startPauses, true; }
    bool lap(TickMs) override { return ++laps, true; }
    bool save(TickMs) override { return true; }
    bool discard(TickMs) override { return true; }
    ActivityState state() const override { return st; }
    ActivityState st = ActivityState::Running;
    int prepares = 0, cancels = 0, startPauses = 0, laps = 0;
};
}

static_assert(std::is_abstract_v<Screen>);
static_assert(!std::is_copy_constructible_v<Screen>);
static_assert(!std::is_polymorphic_v<UiModel>, "view model is plain data");
static_assert(std::is_trivially_copyable_v<UiModel>);

TEST(Navigator, PushPopRunsLifecycleInPairs) {
    Navigator nav;
    TestScreen a("A"), b("B");
    nav.resetTo(a, 0);
    EXPECT_TRUE(nav.push(b, 0));
    EXPECT_EQ(a.exits, 1);
    EXPECT_EQ(b.enters, 1);
    EXPECT_TRUE(nav.pop(0));
    EXPECT_EQ(b.exits, 1);
    EXPECT_EQ(a.enters, 2);
    EXPECT_FALSE(nav.pop(0)) << "root cannot be popped";
    EXPECT_EQ(nav.top(), &a);
}

TEST(Navigator, UnconsumedBackPops) {
    Navigator nav;
    TestScreen a("A"), b("B");
    nav.resetTo(a, 0);
    nav.push(b, 0);
    EXPECT_TRUE(nav.handle(press(KeyId::Back)));
    EXPECT_EQ(nav.top(), &a);
    EXPECT_FALSE(nav.handle(press(KeyId::Back))) << "at root BACK falls through the chain";
}

TEST(Navigator, ConsumedBackDoesNotPop) {
    Navigator nav;
    TestScreen a("A"), b("B", true);
    nav.resetTo(a, 0);
    nav.push(b, 0);
    EXPECT_TRUE(nav.handle(press(KeyId::Back)));
    EXPECT_EQ(nav.top(), &b);
}

TEST(Navigator, DepthLimitAndDuplicates) {
    Navigator nav;
    TestScreen s[Navigator::kMaxDepth + 1] = {TestScreen("0"), TestScreen("1"), TestScreen("2"),
                                              TestScreen("3"), TestScreen("4"), TestScreen("5"), TestScreen("6")};
    nav.resetTo(s[0], 0);
    EXPECT_FALSE(nav.push(s[0], 0)) << "already on the stack";
    for (std::size_t i = 1; i < Navigator::kMaxDepth; ++i) EXPECT_TRUE(nav.push(s[i], 0));
    EXPECT_FALSE(nav.push(s[Navigator::kMaxDepth], 0));
    EXPECT_TRUE(nav.popUntil(s[2], 0));
    EXPECT_EQ(nav.depth(), 3u);
    EXPECT_FALSE(nav.popUntil(s[5], 0));
}

TEST(Navigator, EmptyStackIsSafe) {
    Navigator nav;
    fakes::FakeCanvas c;
    EXPECT_FALSE(nav.handle(press(KeyId::Start)));
    nav.render(c, UiModel{});
    EXPECT_EQ(c.clears, 0);
}

TEST(MenuScreen, NavigatesAndExecutesSelectedCommand) {
    MenuScreen menu("MENU");
    CountingCommand run, settings;
    menu.addItem("Run", run);
    menu.addItem("Settings", settings);
    menu.enter(0);
    EXPECT_TRUE(menu.handle(press(KeyId::Down)));
    EXPECT_TRUE(menu.handle(press(KeyId::Start, Gesture::Short, 1234)));
    EXPECT_EQ(settings.runs, 1);
    EXPECT_EQ(settings.at, 1234u) << "command receives the gesture time";
    EXPECT_FALSE(menu.handle(press(KeyId::Back))) << "menu leaves BACK to the navigator";
}

TEST(MenuScreen, SelectionStopsAtBothEndsWithoutWrapping) {
    MenuScreen menu("PAUSED");
    CountingCommand a, b, c;
    menu.addItem("Resume", a);
    menu.addItem("Save", b);
    menu.addItem("Discard", c);
    menu.enter(0);

    EXPECT_TRUE(menu.handle(press(KeyId::Up))) << "consumed at the top, not passed on";
    EXPECT_EQ(menu.selected(), 0u) << "no wrap to the last item";

    for (int i = 0; i < 5; ++i) EXPECT_TRUE(menu.handle(press(KeyId::Down)));
    EXPECT_EQ(menu.selected(), 2u) << "stops at the last item";

    EXPECT_TRUE(menu.handle(press(KeyId::Up)));
    EXPECT_EQ(menu.selected(), 1u);
}

TEST(MenuScreen, SingleItemMenuNeverMoves) {
    MenuScreen menu("ONE");
    CountingCommand a;
    menu.addItem("Only", a);
    menu.enter(0);
    EXPECT_TRUE(menu.handle(press(KeyId::Down)));
    EXPECT_TRUE(menu.handle(press(KeyId::Up)));
    EXPECT_EQ(menu.selected(), 0u);
}

TEST(MenuScreen, ShowsOnlyNeighboursThatExist) {
    MenuScreen menu("PAUSED");
    CountingCommand a, b, c;
    menu.addItem("Resume", a);
    menu.addItem("Save", b);
    menu.addItem("Discard", c);
    menu.enter(0);
    fakes::FakeCanvas canvas;

    menu.render(canvas, UiModel{});
    EXPECT_TRUE(canvas.shows("PAUSED"));
    EXPECT_TRUE(canvas.shows("Resume"));
    EXPECT_TRUE(canvas.shows("Save")) << "next item below";
    EXPECT_FALSE(canvas.shows("Discard")) << "top of the list: nothing above";

    menu.handle(press(KeyId::Down));
    menu.render(canvas, UiModel{});
    EXPECT_TRUE(canvas.shows("Resume"));
    EXPECT_TRUE(canvas.shows("Save"));
    EXPECT_TRUE(canvas.shows("Discard")) << "middle: both neighbours";

    menu.handle(press(KeyId::Down));
    menu.render(canvas, UiModel{});
    EXPECT_FALSE(canvas.shows("Resume")) << "bottom of the list: nothing below";
    EXPECT_TRUE(canvas.shows("Save")) << "previous item above";
    EXPECT_TRUE(canvas.shows("Discard"));
}

static_assert(themed(Color::Black, Theme::Light) == Color::White, "light theme: white background");
static_assert(themed(Color::White, Theme::Light) == Color::Black, "light theme: dark text");
static_assert(themed(Color::Yellow, Theme::Dark) == Color::Yellow, "dark theme is the identity");
static_assert(!std::is_copy_constructible_v<ThemedCanvas>, "decorator wraps a reference");

TEST(Theme, LightThemeKeepsEveryColourReadableOnWhite) {
    const Color all[] = {Color::Black, Color::White,  Color::LightGray, Color::Gray,  Color::DarkGray,
                         Color::Red,   Color::Orange, Color::Yellow,    Color::Green, Color::Blue,
                         Color::Cyan,  Color::Zone1,  Color::Zone2,     Color::Zone3, Color::Zone4,
                         Color::Zone5, Color::Zone1Dim, Color::Zone2Dim, Color::Zone3Dim, Color::Zone4Dim,
                         Color::Zone5Dim};
    for (Color c : all) {
        EXPECT_EQ(themed(c, Theme::Dark), c);
        if (c == Color::Black) continue;
        const Color l = themed(c, Theme::Light);
        EXPECT_NE(l, Color::White) << "would vanish on white: " << static_cast<int>(c);
        EXPECT_NE(l, Color::Yellow) << static_cast<int>(c);
        EXPECT_NE(l, Color::Cyan) << static_cast<int>(c);
        EXPECT_NE(l, Color::Zone1) << static_cast<int>(c);
    }
}

TEST(Theme, ThemedCanvasForwardsThroughThePalette) {
    fakes::FakeCanvas inner;
    ThemedCanvas light(inner, Theme::Light);
    EXPECT_EQ(light.width(), inner.width());
    light.clear(Color::Black);
    light.drawText(0, 0, "12:34", Font::Large, Color::White, Align::Center);
    EXPECT_EQ(inner.background, Color::White);
    EXPECT_EQ(inner.colorOf("12:34"), Color::Black);

    ThemedCanvas dark(inner, Theme::Dark);
    dark.clear(Color::Black);
    dark.drawText(0, 0, "12:34", Font::Large, Color::White, Align::Center);
    EXPECT_EQ(inner.background, Color::Black);
    EXPECT_EQ(inner.colorOf("12:34"), Color::White);
}

TEST(ActivityDataScreen, BackMeansLapCancelOrNothingDependingOnState) {
    FakeControl ctl;
    ActivityDataScreen screen(ctl);
    ctl.st = ActivityState::Running;
    EXPECT_TRUE(screen.handle(press(KeyId::Back)));
    EXPECT_EQ(ctl.laps, 1);
    ctl.st = ActivityState::Ready;
    EXPECT_TRUE(screen.handle(press(KeyId::Back)));
    EXPECT_EQ(ctl.cancels, 1);
    ctl.st = ActivityState::Paused;
    EXPECT_TRUE(screen.handle(press(KeyId::Back))) << "consumed: cannot pop out of an open activity";
    EXPECT_EQ(ctl.laps, 1);
    EXPECT_TRUE(screen.handle(press(KeyId::Start)));
    EXPECT_EQ(ctl.startPauses, 1);
}

TEST(ActivityDataScreen, PagesCycleAndRenderFromModel) {
    FakeControl ctl;
    ActivityDataScreen screen(ctl);
    UiModel m;
    m.activity.state = ActivityState::Running;
    m.activity.elapsedMs = 754000;
    m.activity.distanceDm = 34200;
    m.activity.currentPaceSecKm = 312;
    m.hrValid = true;
    m.hrBpm = 151;
    m.hrZone = 3;

    fakes::FakeCanvas c;
    screen.render(c, m);
    EXPECT_TRUE(c.shows("12:34"));
    EXPECT_TRUE(c.shows("3.42"));
    EXPECT_TRUE(c.shows("5:12"));

    screen.handle(press(KeyId::Down));
    EXPECT_EQ(screen.page(), ActivityDataScreen::Page::HeartRate);
    screen.render(c, m);
    EXPECT_TRUE(c.shows("151"));
    EXPECT_TRUE(c.shows("ZONE 3"));

    screen.handle(press(KeyId::Up));
    screen.handle(press(KeyId::Up));
    EXPECT_EQ(screen.page(), ActivityDataScreen::Page::Lap);
}

TEST(SettingsScreen, EditModeChangesValuesThroughEditor) {
    EventBus bus;
    SettingsStore store(bus);
    SettingsScreen screen(store);
    screen.enter(0);
    screen.handle(press(KeyId::Start));
    EXPECT_TRUE(screen.editing());
    screen.handle(press(KeyId::Up));
    EXPECT_EQ(store.settings().maxHr, 191);
    EXPECT_TRUE(screen.handle(press(KeyId::Back))) << "first BACK leaves edit mode";
    EXPECT_FALSE(screen.editing());
    EXPECT_FALSE(screen.handle(press(KeyId::Back))) << "second BACK leaves the screen";

    screen.handle(press(KeyId::Down));
    screen.handle(press(KeyId::Start));
    for (int i = 0; i < 10; ++i) screen.handle(press(KeyId::Up));
    EXPECT_EQ(store.settings().targetZone, 5) << "editor clamps by rejecting";
}

TEST(Format, DurationsDistancesPaces) {
    char b[16];
    EXPECT_STREQ(formatDuration(0, b, sizeof b), "0:00");
    EXPECT_STREQ(formatDuration(65000, b, sizeof b), "1:05");
    EXPECT_STREQ(formatDuration(3723000, b, sizeof b), "1:02:03");
    EXPECT_STREQ(formatDistanceKm(34299, b, sizeof b), "3.42");
    EXPECT_STREQ(formatDistanceKm(0, b, sizeof b), "0.00");
    EXPECT_STREQ(formatPace(0, b, sizeof b), "--:--");
    EXPECT_STREQ(formatPace(312, b, sizeof b), "5:12");
}
