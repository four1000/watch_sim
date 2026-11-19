#include "ap/ui/screens.h"

namespace ap {

bool MenuScreen::addItem(const char* label, ICommand& cmd) {
    if (m_count == kMaxItems || label == nullptr) return false;
    m_items[m_count++] = Item{label, &cmd};
    return true;
}

void MenuScreen::onEnter(TickMs) { m_selected = 0; }

bool MenuScreen::onGesture(const GestureEvent& e) {
    if (e.gesture != Gesture::Short || m_count == 0) return false;
    switch (e.key) {
        case KeyId::Up:
            if (m_selected > 0) --m_selected;
            return true;
        case KeyId::Down:
            if (m_selected + 1 < m_count) ++m_selected;
            return true;
        case KeyId::Start:
            if (ICommand* cmd = m_items[m_selected].cmd) cmd->execute(e.timestampMs);
            return true;
        default:
            return false;
    }
}

void MenuScreen::onRender(ICanvas& c, const UiModel&) {
    const int16_t cx = static_cast<int16_t>(c.width() / 2);
    const int16_t cy = static_cast<int16_t>(c.height() / 2);
    c.drawText(cx, 44, m_title, Font::Small, Color::Gray, Align::Center);
    if (m_count == 0) return;

    c.fillRect(0, static_cast<int16_t>(cy - 22), c.width(), 44, Color::Blue);
    c.drawText(cx, cy, m_items[m_selected].label, Font::Large, Color::White, Align::Center);

    if (m_selected > 0) {
        c.drawText(cx, static_cast<int16_t>(cy - 52), m_items[m_selected - 1].label, Font::Medium, Color::Gray,
                   Align::Center);
    }
    if (m_selected + 1 < m_count) {
        c.drawText(cx, static_cast<int16_t>(cy + 52), m_items[m_selected + 1].label, Font::Medium, Color::Gray,
                   Align::Center);
    }
    drawListPosition(c, m_selected, m_count);
}

}
