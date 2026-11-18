#include "ap/ui/overlay.h"

#include <cstdio>

namespace ap {

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
        case EventType::HrAlert:
            std::snprintf(detail, sizeof detail, "%u bpm  Z%u > Z%u", static_cast<unsigned>(e.alert.bpm),
                          static_cast<unsigned>(e.alert.zone), static_cast<unsigned>(e.alert.targetZone));
            show(t, e.alert.kind == HrAlertKind::AboveTarget ? "HR HIGH" : "HR LOW", detail,
                 e.alert.kind == HrAlertKind::AboveTarget ? Color::Red : Color::Blue, true, kAlertMs);
            m_vibrator.vibrate(VibePattern::Long);
            break;

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
                          uint32_t durationMs) {
    if (this->modal() && !modal) return;
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

}
