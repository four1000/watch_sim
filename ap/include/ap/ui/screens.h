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

class ActivityDetailScreen final : public Screen {
public:
    static constexpr std::size_t kVisibleLaps = 4;

    explicit ActivityDetailScreen(const IActivityHistory& history) : m_history(history) {}
    const char* name() const override { return "ActivityDetail"; }

    void select(std::size_t indexFromNewest) { m_activity = indexFromNewest; }
    std::size_t selectedActivity() const { return m_activity; }
    bool showingSummary() const { return m_scroll == 0; }
    std::size_t firstVisibleLap() const { return m_scroll == 0 ? 0 : m_scroll - 1; }

protected:
    void onEnter(TickMs now) override;
    bool onGesture(const GestureEvent& e) override;
    void onRender(ICanvas& c, const UiModel& m) override;
    bool showsClock() const override { return false; }

private:
    std::size_t lastScroll() const;
    void renderSummary(ICanvas& c, const ActivityRecord& a) const;
    void renderLaps(ICanvas& c, const ActivityRecord& a) const;

    const IActivityHistory& m_history;
    std::size_t m_activity = 0;
    std::size_t m_scroll = 0;
};

class HistoryScreen final : public Screen {
public:
    HistoryScreen(const IActivityHistory& history, Navigator& nav, ActivityDetailScreen& detail)
        : m_history(history), m_nav(nav), m_detail(detail) {}
    const char* name() const override { return "History"; }
    std::size_t selected() const { return m_selected; }

protected:
    void onEnter(TickMs now) override;
    bool onGesture(const GestureEvent& e) override;
    void onRender(ICanvas& c, const UiModel& m) override;
    bool showsClock() const override { return false; }

private:
    void renderEntry(ICanvas& c, std::size_t index, int16_t y, bool selected) const;

    const IActivityHistory& m_history;
    Navigator& m_nav;
    ActivityDetailScreen& m_detail;
    std::size_t m_selected = 0;
};
}
