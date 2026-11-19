#include "ap/ui/overlay.h"

#include <cstdio>

namespace ap {

namespace {
constexpr int16_t kCueCentreY = 30;
constexpr int16_t kCueSpacing = 16;
constexpr int16_t kCueTitleY = 22;
constexpr int16_t kCueHalfWidth = 18;
constexpr int16_t kCueHalfHeight = 9;
constexpr uint8_t kCueThickness = 4;
}

void OverlayManager::attach() {
    m_sub = m_bus.subscribe(*this, topics(EventType::HrAlert, EventType::LapCompleted,
                                          EventType::ActivityStateChanged, EventType::KeyLockChanged));
}

bool OverlayManager::handle(const GestureEvent& e) {
    if (!modal()) return false;
    if (e.gesture == Gesture::Short) hide();
    return true;
}

void OverlayManager::onEvent(const Event& e) {
    char detail[sizeof m_detail];
    char a[12];
    char b[12];
    const TickMs t = e.timestampMs;
    switch (e.type) {
        case EventType::HrAlert: {
            const bool high = e.alert.kind == HrAlertKind::AboveTarget;
            show(t, high ? "SLOW DOWN" : "SPEED UP", "", high ? Color::Red : Color::Blue, true, kAlertMs,
                 high ? Cue::Down : Cue::Up);
            m_vibrator.vibrate(VibePattern::Long);
            break;
        }

        case EventType::LapCompleted: {
            char title[sizeof m_title];
            std::snprintf(title, sizeof title, "LAP %u", static_cast<unsigned>(e.lap.index));
            std::snprintf(detail, sizeof detail, "%s  %s km", formatDuration(e.lap.durationMs, a, sizeof a),
                          formatDistanceKm(e.lap.distanceDm, b, sizeof b));
            show(t, title, detail, Color::Cyan, false, kToastMs);
            m_vibrator.vibrate(VibePattern::Short);
            break;
        }

        case EventType::ActivityStateChanged:
            switch (e.activity.outcome) {
                case ActivityOutcome::Saved:
                    show(t, "SAVED", "activity stored", Color::Green, false, kToastMs);
                    m_vibrator.vibrate(VibePattern::Double);
                    break;
                case ActivityOutcome::SaveFailed:
                    show(t, "SAVE FAILED", "retry or discard", Color::Red, true, kAlertMs);
                    m_vibrator.vibrate(VibePattern::Long);
                    break;
                case ActivityOutcome::Discarded:
                    show(t, "DISCARDED", "", Color::Gray, false, kToastMs);
                    break;
                case ActivityOutcome::StartFailed:
                    show(t, "START FAILED", "GPS / storage error", Color::Red, true, kAlertMs);
                    m_vibrator.vibrate(VibePattern::Long);
                    break;
                case ActivityOutcome::Cancelled:
                case ActivityOutcome::None:
                    if (e.activity.to == ActivityState::Running) m_vibrator.vibrate(VibePattern::Short);
                    if (e.activity.to == ActivityState::Paused) m_vibrator.vibrate(VibePattern::Double);
                    break;
            }
            break;

        case EventType::KeyLockChanged:
            show(t, e.keyLocked ? "LOCKED" : "UNLOCKED", "double LIGHT", Color::Yellow, false, kToastMs);
            break;

        default:
            break;
    }
}

void OverlayManager::onTimer(uint8_t, TickMs) { hide(); }

void OverlayManager::show(TickMs now, const char* title, const char* detail, Color color, bool modal,
                          uint32_t durationMs, Cue cue) {
    if (this->modal() && !modal) return;
    m_cue = cue;
    std::snprintf(m_title, sizeof m_title, "%s", title);
    std::snprintf(m_detail, sizeof m_detail, "%s", detail);
    m_color = color;
    m_modal = modal;
    m_visible = true;
    m_timers.cancel(m_timer);
    m_timer = m_timers.start(now, durationMs, *this, 0);
}

void OverlayManager::hide() {
    m_visible = false;
    m_modal = false;
    m_timers.cancel(m_timer);
}

void OverlayManager::render(ICanvas& c, const UiModel&) const {
    if (!m_visible) return;
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t cy = static_cast<int16_t>(c.height() / 2);
    if (m_modal) {
        c.fillCircle(cx, cy, static_cast<int16_t>(cx - 14), m_color);
        if (m_cue != Cue::None) {
            renderCue(c, cx, static_cast<int16_t>(cy - kCueCentreY - kCueSpacing / 2));
            renderCue(c, cx, static_cast<int16_t>(cy - kCueCentreY + kCueSpacing / 2));
            c.drawText(cx, static_cast<int16_t>(cy + kCueTitleY), m_title, Font::Large, Color::White, Align::Center);
            return;
        }
        c.drawText(cx, static_cast<int16_t>(cy - 18), m_title, Font::Large, Color::White, Align::Center);
        c.drawText(cx, static_cast<int16_t>(cy + 18), m_detail, Font::Small, Color::White, Align::Center);
        c.drawText(cx, static_cast<int16_t>(cy + 48), "any key", Font::Small, Color::LightGray, Align::Center);
    } else {
        c.fillRect(0, static_cast<int16_t>(cy - 34), c.width(), 68, Color::DarkGray);
        c.fillRect(0, static_cast<int16_t>(cy - 34), c.width(), 4, m_color);
        c.drawText(cx, static_cast<int16_t>(cy - 10), m_title, Font::Large, m_color, Align::Center);
        c.drawText(cx, static_cast<int16_t>(cy + 18), m_detail, Font::Small, Color::White, Align::Center);
    }
}

void OverlayManager::renderCue(ICanvas& c, int16_t cx, int16_t y) const {
    const int16_t tipDy = m_cue == Cue::Up ? -kCueHalfHeight : kCueHalfHeight;
    const int16_t tipY = static_cast<int16_t>(y + tipDy);
    const int16_t baseY = static_cast<int16_t>(y - tipDy);
    c.drawLine(static_cast<int16_t>(cx - kCueHalfWidth), baseY, cx, tipY, kCueThickness, Color::White);
    c.drawLine(cx, tipY, static_cast<int16_t>(cx + kCueHalfWidth), baseY, kCueThickness, Color::White);
}

}
