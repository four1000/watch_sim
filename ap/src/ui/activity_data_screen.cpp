#include <cmath>
#include <cstdio>

#include "ap/health/hr_zone_monitor.h"
#include "ap/ui/screens.h"

namespace ap {
namespace {

constexpr uint8_t kPageCount = static_cast<uint8_t>(ActivityDataScreen::Page::Count);
constexpr int kCurrentZoneThickness = 9;
constexpr int kOtherZoneThickness = 3;
}

void ActivityDataScreen::onEnter(TickMs) {
    if (m_control.state() == ActivityState::Ready) m_page = Page::Timer;
}

bool ActivityDataScreen::onGesture(const GestureEvent& e) {
    if (e.gesture != Gesture::Short) return false;
    const uint8_t p = static_cast<uint8_t>(m_page);
    switch (e.key) {
        case KeyId::Up:
            m_page = static_cast<Page>((p + kPageCount - 1) % kPageCount);
            return true;
        case KeyId::Down:
            m_page = static_cast<Page>((p + 1) % kPageCount);
            return true;
        case KeyId::Start:
            m_control.startPause(e.timestampMs);
            return true;
        case KeyId::Back:
            if (m_control.state() == ActivityState::Running) m_control.lap(e.timestampMs);
            else if (m_control.state() == ActivityState::Ready) m_control.cancel(e.timestampMs);
            return true;
        default:
            return false;
    }
}

void ActivityDataScreen::onRender(ICanvas& c, const UiModel& m) {
    renderHeader(c, m);
    switch (m_page) {
        case Page::Timer:     renderTimer(c, m); break;
        case Page::HeartRate: renderHeartRate(c, m); break;
        case Page::Lap:       renderLap(c, m); break;
        case Page::Count:     break;
    }
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    for (uint8_t i = 0; i < kPageCount; ++i) {
        const int16_t x = static_cast<int16_t>(cx + (i - 1) * 12);
        c.fillCircle(x, static_cast<int16_t>(c.height() - 18), 3,
                     i == static_cast<uint8_t>(m_page) ? Color::White : Color::DarkGray);
    }
}

void ActivityDataScreen::renderHeader(ICanvas& c, const UiModel& m) const {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    switch (m.activity.state) {
        case ActivityState::Ready:
            c.drawText(cx, 44, m.activity.gpsFix ? "GPS READY" : "GPS ...", Font::Small,
                       m.activity.gpsFix ? Color::Green : Color::Yellow, Align::Center);
            break;
        case ActivityState::Paused:
            if (m.time.second % 2 == 0) c.drawText(cx, 44, "PAUSED", Font::Small, Color::Yellow, Align::Center);
            break;
        case ActivityState::Running:
            c.drawText(cx, 44, "RUN", Font::Small, Color::Green, Align::Center);
            break;
        case ActivityState::Idle:
            break;
    }
}

void ActivityDataScreen::renderTimer(ICanvas& c, const UiModel& m) const {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    char buf[16];
    c.drawText(cx, 88, formatDuration(m.activity.elapsedMs, buf, sizeof buf), Font::Number,
               m.activity.state == ActivityState::Paused ? Color::Yellow : Color::White, Align::Center);
    c.drawLine(30, 122, static_cast<int16_t>(c.width() - 30), 122, 1, Color::DarkGray);

    c.drawText(static_cast<int16_t>(cx + 20), 148, formatDistanceKm(m.activity.distanceDm, buf, sizeof buf),
               Font::Large, Color::White, Align::Right);
    c.drawText(static_cast<int16_t>(cx + 26), 152, "km", Font::Small, Color::Gray, Align::Left);

    c.drawText(static_cast<int16_t>(cx + 20), 186, formatPace(m.activity.currentPaceSecKm, buf, sizeof buf),
               Font::Large, Color::White, Align::Right);
    c.drawText(static_cast<int16_t>(cx + 26), 190, "/km", Font::Small, Color::Gray, Align::Left);
}

void ActivityDataScreen::renderHeartRate(ICanvas& c, const UiModel& m) const {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t cy = static_cast<int16_t>(c.height() / 2);
    const int16_t r = static_cast<int16_t>(cx - 6);

    constexpr double kDegToRad = 3.14159265358979 / 180.0;
    const auto px = [&](int rad, int deg) { return static_cast<int16_t>(cx + std::lround(rad * std::cos(deg * kDegToRad))); };
    const auto py = [&](int rad, int deg) { return static_cast<int16_t>(cy + std::lround(rad * std::sin(deg * kDegToRad))); };

    constexpr int kStart = 135, kSpan = 270;
    constexpr int kSeg = kSpan / kZoneCount;
    for (uint8_t z = 1; z <= kZoneCount; ++z) {
        const bool current = m.hrValid && z == m.hrZone;
        const bool highlighted = current || z == m.settings.targetZone;
        const int thickness = current ? kCurrentZoneThickness : kOtherZoneThickness;
        const int roundCapDeg = static_cast<int>(std::ceil((thickness / 2.0) / (r * kDegToRad)));
        const int a0 = kStart + (z - 1) * kSeg;
        c.drawArc(cx, cy, r, static_cast<int16_t>(a0 + roundCapDeg), static_cast<int16_t>(a0 + kSeg - roundCapDeg),
                  static_cast<uint8_t>(thickness), highlighted ? zoneColor(z) : zoneColorDim(z));
    }

    char buf[24];
    if (m.hrValid) {
        const uint8_t maxHr = m.settings.maxHr;
        const int lo = hrZoneLowerBound(1, maxHr);
        const int clamped = m.hrBpm < lo ? lo : (m.hrBpm > maxHr ? maxHr : m.hrBpm);
        const int angle = kStart + (clamped - lo) * kSpan / (maxHr - lo > 0 ? maxHr - lo : 1);
        c.fillCircle(px(r, angle), py(r, angle), 7, Color::Black);
        c.fillCircle(px(r, angle), py(r, angle), 5, Color::White);

        std::snprintf(buf, sizeof buf, "%u", static_cast<unsigned>(m.hrBpm));
        c.drawText(cx, static_cast<int16_t>(cy - 10), buf, Font::Number, Color::White, Align::Center);
        std::snprintf(buf, sizeof buf, "ZONE %u", static_cast<unsigned>(m.hrZone));
        c.drawText(cx, static_cast<int16_t>(cy + 28), buf, Font::Medium, zoneColor(m.hrZone), Align::Center);
    } else {
        c.drawText(cx, static_cast<int16_t>(cy - 10), "--", Font::Number, Color::Gray, Align::Center);
        c.drawText(cx, static_cast<int16_t>(cy + 28), "NO HR", Font::Medium, Color::Gray, Align::Center);
    }
    std::snprintf(buf, sizeof buf, "TARGET Z%u", static_cast<unsigned>(m.settings.targetZone));
    c.drawText(cx, static_cast<int16_t>(cy + 56), buf, Font::Small, Color::LightGray, Align::Center);
}

void ActivityDataScreen::renderLap(ICanvas& c, const UiModel& m) const {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    char buf[24];
    std::snprintf(buf, sizeof buf, "LAP %u", static_cast<unsigned>(m.activity.lapCount + 1u));
    c.drawText(cx, 70, buf, Font::Medium, Color::Cyan, Align::Center);
    c.drawText(cx, 108, formatDuration(m.activity.lapElapsedMs, buf, sizeof buf), Font::Number, Color::White,
               Align::Center);

    char dist[12];
    std::snprintf(buf, sizeof buf, "%s km", formatDistanceKm(m.activity.lapDistanceDm, dist, sizeof dist));
    c.drawText(cx, 148, buf, Font::Medium, Color::White, Align::Center);

    if (m.activity.hasLastLap) {
        char t[12];
        std::snprintf(buf, sizeof buf, "LAST %s", formatDuration(m.activity.lastLap.durationMs, t, sizeof t));
        c.drawText(cx, 182, buf, Font::Small, Color::LightGray, Align::Center);
    }
}

}
