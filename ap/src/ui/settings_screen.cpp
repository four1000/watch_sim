#include <cstdio>

#include "ap/ui/screens.h"

namespace ap {
namespace {

constexpr uint8_t kRowCount = static_cast<uint8_t>(SettingsScreen::Row::Count);
}

void SettingsScreen::onEnter(TickMs) {
    m_row = Row::MaxHr;
    m_editing = false;
}

bool SettingsScreen::onGesture(const GestureEvent& e) {
    if (e.gesture != Gesture::Short) return false;
    const uint8_t r = static_cast<uint8_t>(m_row);
    switch (e.key) {
        case KeyId::Up:
            if (m_editing) adjust(+1, e.timestampMs);
            else m_row = static_cast<Row>((r + kRowCount - 1) % kRowCount);
            return true;
        case KeyId::Down:
            if (m_editing) adjust(-1, e.timestampMs);
            else m_row = static_cast<Row>((r + 1) % kRowCount);
            return true;
        case KeyId::Start:
            if (m_row == Row::HrAlerts) {
                m_editor.setHrAlerts(!m_editor.settings().hrAlerts, e.timestampMs);
            } else {
                m_editing = !m_editing;
            }
            return true;
        case KeyId::Back:
            if (m_editing) {
                m_editing = false;
                return true;
            }
            return false;
        default:
            return false;
    }
}

void SettingsScreen::adjust(int delta, TickMs now) {
    const UserSettings& s = m_editor.settings();
    switch (m_row) {
        case Row::MaxHr:
            m_editor.setMaxHr(static_cast<uint8_t>(s.maxHr + delta), now);
            break;
        case Row::TargetZone:
            m_editor.setTargetZone(static_cast<uint8_t>(s.targetZone + delta), now);
            break;
        case Row::HrAlerts:
        case Row::Count:
            break;
    }
}

void SettingsScreen::onRender(ICanvas& c, const UiModel& m) {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    c.drawText(cx, 50, "SETTINGS", Font::Small, Color::Gray, Align::Center);

    char values[kRowCount][12];
    std::snprintf(values[0], sizeof values[0], "%u", static_cast<unsigned>(m.settings.maxHr));
    std::snprintf(values[1], sizeof values[1], "Z%u", static_cast<unsigned>(m.settings.targetZone));
    std::snprintf(values[2], sizeof values[2], "%s", m.settings.hrAlerts ? "ON" : "OFF");
    const char* const labels[kRowCount] = {"Max HR", "Target", "HR Alert"};

    for (uint8_t i = 0; i < kRowCount; ++i) {
        const int16_t y = static_cast<int16_t>(88 + i * 40);
        const bool sel = (i == static_cast<uint8_t>(m_row));
        if (sel) c.fillRect(0, static_cast<int16_t>(y - 18), c.width(), 36, m_editing ? Color::DarkGray : Color::Blue);
        c.drawText(40, y, labels[i], Font::Medium, Color::White, Align::Left);
        c.drawText(static_cast<int16_t>(c.width() - 40), y, values[i], Font::Medium,
                   sel && m_editing ? Color::Yellow : Color::White, Align::Right);
    }
    if (m_editing) c.drawText(cx, 210, "UP/DOWN change", Font::Small, Color::Yellow, Align::Center);
}

}
