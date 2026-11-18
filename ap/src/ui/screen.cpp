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
void Screen::drawStatusBar(ICanvas& c, const UiModel& m) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02u:%02u", static_cast<unsigned>(m.time.hour), static_cast<unsigned>(m.time.minute));
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    c.drawText(cx, 20, buf, Font::Small, Color::LightGray, Align::Center);

    if (m.activity.state != ActivityState::Idle) {
        c.fillCircle(static_cast<int16_t>(cx - 34), 20, 4, m.activity.gpsFix ? Color::Green : Color::Red);
    }
    c.fillCircle(static_cast<int16_t>(cx + 34), 20, 4, m.hrValid ? zoneColor(m.hrZone) : Color::DarkGray);
    if (m.keyLocked) c.drawText(static_cast<int16_t>(cx + 48), 20, "L", Font::Small, Color::Yellow, Align::Center);
}

}  // namespace ap
