#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/activity/activity_session.h"
#include "ap/app/settings.h"
#include "ap/input/input_router.h"
#include "ap/ui/navigator.h"
#include "ap/ui/screen.h"

namespace ap {

class WatchFaceScreen final : public Screen {
public:
    WatchFaceScreen(Navigator& nav, Screen& activityMenu) : m_nav(nav), m_menu(activityMenu) {}
    const char* name() const override { return "WatchFace"; }

protected:
    bool onGesture(const GestureEvent& e) override;
    void onRender(ICanvas& c, const UiModel& m) override;
    bool showsStatusBar() const override { return false; }

private:
    Navigator& m_nav;
    Screen& m_menu;
};

class MenuScreen final : public Screen {
public:
    static constexpr std::size_t kMaxItems = 6;

    explicit MenuScreen(const char* title, bool showClock = true) : m_title(title), m_showClock(showClock) {}
    bool addItem(const char* label, ICommand& cmd);

    const char* name() const override { return m_title; }
    std::size_t selected() const { return m_selected; }
    std::size_t itemCount() const { return m_count; }

protected:
    void onEnter(TickMs now) override;
    bool onGesture(const GestureEvent& e) override;
    void onRender(ICanvas& c, const UiModel& m) override;
    bool showsClock() const override { return m_showClock; }

private:
    struct Item {
        const char* label = nullptr;
        ICommand* cmd = nullptr;
    };
    const char* m_title;
    bool m_showClock;
    std::array<Item, kMaxItems> m_items{};
    std::size_t m_count = 0;
    std::size_t m_selected = 0;
};

class ActivityDataScreen final : public Screen {
public:
    enum class Page : uint8_t { Timer, HeartRate, Lap, Count };

    explicit ActivityDataScreen(IActivityControl& control) : m_control(control) {}
    const char* name() const override { return "ActivityData"; }
    Page page() const { return m_page; }

protected:
    void onEnter(TickMs now) override;
    bool onGesture(const GestureEvent& e) override;
    void onRender(ICanvas& c, const UiModel& m) override;
    bool showsClock() const override { return false; }

private:
    void renderHeader(ICanvas& c, const UiModel& m) const;
    void renderTimer(ICanvas& c, const UiModel& m) const;
    void renderHeartRate(ICanvas& c, const UiModel& m) const;
    void renderLap(ICanvas& c, const UiModel& m) const;

    IActivityControl& m_control;
    Page m_page = Page::Timer;
};

class SettingsScreen final : public Screen {
public:
    enum class Row : uint8_t { MaxHr, TargetZone, HrAlerts, Count };

    explicit SettingsScreen(ISettingsEditor& editor) : m_editor(editor) {}
    const char* name() const override { return "Settings"; }
    Row row() const { return m_row; }
    bool editing() const { return m_editing; }

protected:
    void onEnter(TickMs now) override;
    bool onGesture(const GestureEvent& e) override;
    void onRender(ICanvas& c, const UiModel& m) override;
    bool showsClock() const override { return false; }

private:
    void adjust(int delta, TickMs now);

    ISettingsEditor& m_editor;
    Row m_row = Row::MaxHr;
    bool m_editing = false;
};
}
