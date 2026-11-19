#include <cstdio>

#include "ap/ui/screens.h"

namespace ap {

namespace {
constexpr int16_t kDateY = 52;
constexpr int16_t kLapHeaderY = 78;
constexpr int16_t kFirstLapRowY = 104;
constexpr int16_t kLapRowStep = 28;
constexpr int16_t kLapNumberX = 52;
constexpr int16_t kLapTimeX = 122;
constexpr int16_t kLapKmX = 168;
constexpr int16_t kLapPaceX = 214;
constexpr const char* kUnknownLapPace = "--";
}

void ActivityDetailScreen::onEnter(TickMs) { m_scroll = 0; }

std::size_t ActivityDetailScreen::lastScroll() const {
    ActivityRecord a{};
    if (!m_history.activityAt(m_activity, a) || a.summary.laps == 0) return 0;
    const std::size_t laps = a.summary.laps;
    return 1 + (laps > kVisibleLaps ? laps - kVisibleLaps : 0);
}

bool ActivityDetailScreen::onGesture(const GestureEvent& e) {
    if (e.gesture != Gesture::Short) return false;
    switch (e.key) {
        case KeyId::Up:
            if (m_scroll > 0) --m_scroll;
            return true;
        case KeyId::Down:
            if (m_scroll < lastScroll()) ++m_scroll;
            return true;
        default:
            return false;
    }
}

void ActivityDetailScreen::onRender(ICanvas& c, const UiModel&) {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    ActivityRecord a{};
    if (!m_history.activityAt(m_activity, a)) {
        c.drawText(cx, static_cast<int16_t>(c.height() / 2), "NO DATA", Font::Medium, Color::Gray, Align::Center);
        return;
    }
    char buf[24];
    c.drawText(cx, kDateY, formatDayMonthTime(a.start, buf, sizeof buf), Font::Small, Color::Gray, Align::Center);

    if (showingSummary()) renderSummary(c, a);
    else renderLaps(c, a);

    const std::size_t last = lastScroll();
    if (last > 0) drawListPosition(c, m_scroll, last + 1);
}

void ActivityDetailScreen::renderSummary(ICanvas& c, const ActivityRecord& a) const {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const ActivitySummary& s = a.summary;
    char buf[24];
    char value[12];

    c.drawText(cx, 96, formatDuration(s.elapsedMs, buf, sizeof buf), Font::Number, Color::White, Align::Center);

    std::snprintf(buf, sizeof buf, "%s km", formatDistanceKm(s.distanceDm, value, sizeof value));
    c.drawText(cx, 138, buf, Font::Medium, Color::White, Align::Center);

    std::snprintf(buf, sizeof buf, "%s /km", formatPace(paceSecPerKm(s.elapsedMs, s.distanceDm), value, sizeof value));
    c.drawText(cx, 166, buf, Font::Medium, Color::White, Align::Center);

    if (s.avgHr > 0) {
        std::snprintf(buf, sizeof buf, "HR %u / %u", static_cast<unsigned>(s.avgHr), static_cast<unsigned>(s.maxHr));
    } else {
        std::snprintf(buf, sizeof buf, "HR --");
    }
    c.drawText(cx, 192, buf, Font::Small, Color::LightGray, Align::Center);

    if (s.laps > 0) {
        std::snprintf(buf, sizeof buf, "%u LAPS", static_cast<unsigned>(s.laps));
        c.drawText(cx, 214, buf, Font::Small, Color::Cyan, Align::Center);
    }
}

void ActivityDetailScreen::renderLaps(ICanvas& c, const ActivityRecord& a) const {
    c.drawText(kLapNumberX, kLapHeaderY, "LAP", Font::Small, Color::Gray, Align::Right);
    c.drawText(kLapTimeX, kLapHeaderY, "TIME", Font::Small, Color::Gray, Align::Right);
    c.drawText(kLapKmX, kLapHeaderY, "KM", Font::Small, Color::Gray, Align::Right);
    c.drawText(kLapPaceX, kLapHeaderY, "PACE", Font::Small, Color::Gray, Align::Right);

    char buf[12];
    for (std::size_t row = 0; row < kVisibleLaps; ++row) {
        const std::size_t lapIndex = firstVisibleLap() + row;
        LapRecord lap{};
        if (lapIndex >= a.summary.laps || !m_history.lapAt(m_activity, lapIndex, lap)) break;
        const int16_t y = static_cast<int16_t>(kFirstLapRowY + static_cast<int16_t>(row) * kLapRowStep);

        std::snprintf(buf, sizeof buf, "%u", static_cast<unsigned>(lap.index));
        c.drawText(kLapNumberX, y, buf, Font::Small, Color::Cyan, Align::Right);
        c.drawText(kLapTimeX, y, formatDuration(lap.durationMs, buf, sizeof buf), Font::Small, Color::White, Align::Right);
        c.drawText(kLapKmX, y, formatDistanceKm(lap.distanceDm, buf, sizeof buf), Font::Small, Color::White,
                   Align::Right);
        const uint16_t pace = paceSecPerKm(lap.durationMs, lap.distanceDm);
        c.drawText(kLapPaceX, y, pace == 0 ? kUnknownLapPace : formatPace(pace, buf, sizeof buf), Font::Small,
                   Color::White, Align::Right);
    }
}

}
