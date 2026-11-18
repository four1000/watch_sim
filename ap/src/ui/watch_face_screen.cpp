#include <cstdio>

#include "ap/ui/screens.h"

namespace ap {

namespace {
const char* const kWeekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                               "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
}  // namespace

bool WatchFaceScreen::onGesture(const GestureEvent& e) {
    if (e.key == KeyId::Start && e.gesture == Gesture::Short) {
        return m_nav.push(m_menu, e.timestampMs);
    }
    return false;
}

void WatchFaceScreen::onRender(ICanvas& c, const UiModel& m) {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t cy = static_cast<int16_t>(c.height() / 2);
    const int16_t r = static_cast<int16_t>(cx - 8);

    // Seconds ring: background + progress from 12 o'clock.
    c.drawArc(cx, cy, r, 0, 360, 4, Color::DarkGray);
    if (m.time.second > 0) {
        c.drawArc(cx, cy, r, -90, static_cast<int16_t>(-90 + 6 * m.time.second), 4, Color::Cyan);
    }
    // START key hint at 2 o'clock.
    c.drawArc(cx, cy, static_cast<int16_t>(r + 4), -40, -20, 4, Color::Green);

    char buf[24];
    std::snprintf(buf, sizeof buf, "%02u:%02u", static_cast<unsigned>(m.time.hour), static_cast<unsigned>(m.time.minute));
    c.drawText(cx, static_cast<int16_t>(cy - 12), buf, Font::Number, Color::White, Align::Center);

    const unsigned wd = m.time.weekday < 7 ? m.time.weekday : 0u;
    const unsigned mo = (m.time.month >= 1 && m.time.month <= 12) ? m.time.month - 1u : 0u;
    std::snprintf(buf, sizeof buf, "%s %u %s", kWeekdays[wd], static_cast<unsigned>(m.time.day), kMonths[mo]);
    c.drawText(cx, static_cast<int16_t>(cy + 32), buf, Font::Small, Color::LightGray, Align::Center);

    if (m.hrValid) std::snprintf(buf, sizeof buf, "HR %u", static_cast<unsigned>(m.hrBpm));
    else           std::snprintf(buf, sizeof buf, "HR --");
    c.drawText(cx, static_cast<int16_t>(cy + 62), buf, Font::Medium,
               m.hrValid ? zoneColor(m.hrZone) : Color::Gray, Align::Center);

    if (m.keyLocked) c.drawText(cx, static_cast<int16_t>(cy - 62), "LOCKED", Font::Small, Color::Yellow, Align::Center);
}

}  // namespace ap
