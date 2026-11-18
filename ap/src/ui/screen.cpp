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
    const uint32_t tenMetres = dm / 100u;  // truncate, like a watch does
    std::snprintf(buf, len, "%lu.%02lu", static_cast<unsigned long>(tenMetres / 100u),
                  static_cast<unsigned long>(tenMetres % 100u));
    return buf;
}

const char* formatPace(uint16_t secPerKm, char* buf, std::size_t len) {
    if (secPerKm == 0) std::snprintf(buf, len, "--:--");
    else std::snprintf(buf, len, "%u:%02u", static_cast<unsigned>(secPerKm / 60u), static_cast<unsigned>(secPerKm % 60u));
    return buf;
}

// Fixed part of the template method: same chrome on every screen.
//
// Layout, symmetric around the centre:   GPS ●  [hh:mm]  ● HR  L
// The dots carry the state (colour), the labels say what the dot is.
void Screen::drawStatusBar(ICanvas& c, const UiModel& m, bool clock) {
    constexpr int16_t kY = 26;
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    // Half-width kept free in the middle: room for the clock, or just a small gap.
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
        // Without the clock the row is narrow: lock goes after HR. With the
        // clock the right side is too close to the round edge: it goes left.
        if (!clock) {
            c.drawText(static_cast<int16_t>(dotR + 28), kY, "L", Font::Small, Color::Yellow, Align::Left);
        } else {
            c.drawText(static_cast<int16_t>(gps ? dotL - 40 : dotL + 3), kY, "L", Font::Small, Color::Yellow,
                       Align::Right);
        }
    }
}

}  // namespace ap
