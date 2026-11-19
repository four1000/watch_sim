#include <cstdio>

#include "ap/ui/screens.h"

namespace ap {

namespace {
constexpr int16_t kTitleY = 44;
constexpr int16_t kNeighbourOffset = 46;
constexpr int16_t kSelectedBarHalfHeight = 26;
}

void HistoryScreen::onEnter(TickMs) { m_selected = 0; }

bool HistoryScreen::onGesture(const GestureEvent& e) {
    if (e.gesture != Gesture::Short) return false;
    const std::size_t count = m_history.activityCount();
    switch (e.key) {
        case KeyId::Up:
            if (m_selected > 0) --m_selected;
            return true;
        case KeyId::Down:
            if (m_selected + 1 < count) ++m_selected;
            return true;
        case KeyId::Start:
            if (m_selected < count) {
                m_detail.select(m_selected);
                m_nav.push(m_detail, e.timestampMs);
            }
            return true;
        default:
            return false;
    }
}

void HistoryScreen::onRender(ICanvas& c, const UiModel&) {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t cy = static_cast<int16_t>(c.height() / 2);
    c.drawText(cx, kTitleY, "HISTORY", Font::Small, Color::Gray, Align::Center);

    const std::size_t count = m_history.activityCount();
    if (count == 0) {
        c.drawText(cx, cy, "NO ACTIVITIES", Font::Medium, Color::Gray, Align::Center);
        return;
    }
    const std::size_t selected = m_selected < count ? m_selected : count - 1;

    c.fillRect(0, static_cast<int16_t>(cy - kSelectedBarHalfHeight), c.width(),
               static_cast<int16_t>(2 * kSelectedBarHalfHeight), Color::Blue);
    renderEntry(c, selected, cy, true);
    if (selected > 0) renderEntry(c, selected - 1, static_cast<int16_t>(cy - kNeighbourOffset), false);
    if (selected + 1 < count) renderEntry(c, selected + 1, static_cast<int16_t>(cy + kNeighbourOffset), false);
    drawListPosition(c, selected, count);
}

void HistoryScreen::renderEntry(ICanvas& c, std::size_t index, int16_t y, bool selected) const {
    ActivityRecord a{};
    if (!m_history.activityAt(index, a)) return;
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    char date[24];
    formatDayMonthTime(a.start, date, sizeof date);
    if (!selected) {
        c.drawText(cx, y, date, Font::Small, Color::Gray, Align::Center);
        return;
    }
    c.drawText(cx, static_cast<int16_t>(y - 9), date, Font::Medium, Color::White, Align::Center);

    char km[12];
    char time[12];
    char stats[32];
    std::snprintf(stats, sizeof stats, "%s km  %s", formatDistanceKm(a.summary.distanceDm, km, sizeof km),
                  formatDuration(a.summary.elapsedMs, time, sizeof time));
    c.drawText(cx, static_cast<int16_t>(y + 14), stats, Font::Small, Color::LightGray, Align::Center);
}

}
