#include "ap/ui/screen.h"

#include <cstdio>

namespace ap {

const char* formatDuration(uint32_t ms, char* buf, std::size_t len) {
    const uint32_t total = ms / 1000u;
    const unsigned h = static_cast<unsigned>(total / 3600u);
    const unsigned m = static_cast<unsigned>((total / 60u) % 60u);
    const unsigned s = static_cast<unsigned>(total % 60u);
    if (h > 0) std::snprintf(buf, len, "%u:%02u:%02u", h, m, s);
    else       std::snprintf(buf, len, "%u:%02u", m, s);
    return buf;
}

const char* formatDistanceKm(uint32_t dm, char* buf, std::size_t len) {
    const uint32_t tenMetres = dm / 100u;
    std::snprintf(buf, len, "%lu.%02lu", static_cast<unsigned long>(tenMetres / 100u),
                  static_cast<unsigned long>(tenMetres % 100u));
    return buf;
}

const char* formatPace(uint16_t secPerKm, char* buf, std::size_t len) {
    if (secPerKm == 0) std::snprintf(buf, len, "--:--");
    else std::snprintf(buf, len, "%u:%02u", static_cast<unsigned>(secPerKm / 60u), static_cast<unsigned>(secPerKm % 60u));
    return buf;
}

const char* monthAbbrev(uint8_t month) {
    static const char* const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    return (month >= 1 && month <= 12) ? kMonths[month - 1] : "---";
}

const char* formatDayMonthTime(const LocalTime& t, char* buf, std::size_t len) {
    std::snprintf(buf, len, "%u %s %02u:%02u", static_cast<unsigned>(t.day), monthAbbrev(t.month),
                  static_cast<unsigned>(t.hour), static_cast<unsigned>(t.minute));
    return buf;
}

uint16_t paceSecPerKm(uint32_t durationMs, uint32_t distanceDm) {
    constexpr uint64_t kSlowestShownSecPerKm = 99u * 60u + 59u;
    if (distanceDm == 0) return 0;
    const uint64_t secPerKm = static_cast<uint64_t>(durationMs) * 10u / distanceDm;
    return secPerKm > kSlowestShownSecPerKm ? 0 : static_cast<uint16_t>(secPerKm);
}

void Screen::drawListPosition(ICanvas& c, std::size_t index, std::size_t count) {
    if (count == 0) return;
    constexpr int kSpanDeg = 60;
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t cy = static_cast<int16_t>(c.height() / 2);
    const int16_t r = static_cast<int16_t>(cx - 6);
    const int seg = kSpanDeg / static_cast<int>(count);
    const int start = -kSpanDeg / 2 + seg * static_cast<int>(index);
    c.drawArc(cx, cy, r, -kSpanDeg / 2, kSpanDeg / 2, 3, Color::DarkGray);
    c.drawArc(cx, cy, r, static_cast<int16_t>(start), static_cast<int16_t>(start + seg), 3, Color::White);
}

void Screen::drawStatusBar(ICanvas& c, const UiModel& m, bool clock) {
    constexpr int16_t kY = 26;
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t gap = clock ? 32 : 6;

    if (clock) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "%02u:%02u", static_cast<unsigned>(m.time.hour),
                      static_cast<unsigned>(m.time.minute));
        c.drawText(cx, kY, buf, Font::Small, Color::LightGray, Align::Center);
    }

    const int16_t dotL = static_cast<int16_t>(cx - gap - 4);
    const int16_t dotR = static_cast<int16_t>(cx + gap + 4);
    const bool gps = m.activity.state != ActivityState::Idle;
    if (gps) {
        c.fillCircle(dotL, kY, 3, m.activity.gpsFix ? Color::Green : Color::Red);
        c.drawText(static_cast<int16_t>(dotL - 6), kY, "GPS", Font::Small, Color::Gray, Align::Right);
    }
    c.fillCircle(dotR, kY, 3, m.hrValid ? zoneColor(m.hrZone) : Color::DarkGray);
    c.drawText(static_cast<int16_t>(dotR + 6), kY, "HR", Font::Small, Color::Gray, Align::Left);

    if (m.keyLocked) {
        if (!clock) {
            c.drawText(static_cast<int16_t>(dotR + 28), kY, "L", Font::Small, Color::Yellow, Align::Left);
        } else {
            c.drawText(static_cast<int16_t>(gps ? dotL - 40 : dotL + 3), kY, "L", Font::Small, Color::Yellow,
                       Align::Right);
        }
    }
}

}
