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
        // Linear list: the selection stops at the first / last item. The key is
        // still consumed at the edge, so it never falls through to another handler.
        case KeyId::Up:
            if (m_selected > 0) --m_selected;
            return true;
        case KeyId::Down:
            if (m_selected + 1 < m_count) ++m_selected;
            return true;
        case KeyId::Start:
            // The command may navigate (push/pop); nothing below touches members.
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

    // Neighbours only where they exist: nothing above the first item, nothing below the last.
    if (m_selected > 0) {
        c.drawText(cx, static_cast<int16_t>(cy - 52), m_items[m_selected - 1].label, Font::Medium, Color::Gray,
                   Align::Center);
    }
    if (m_selected + 1 < m_count) {
        c.drawText(cx, static_cast<int16_t>(cy + 52), m_items[m_selected + 1].label, Font::Medium, Color::Gray,
                   Align::Center);
    }
    // Position indicator on the right edge (follows the round bezel).
    const int16_t r = static_cast<int16_t>(cx - 6);
    const int span = 60;
    const int seg = span / static_cast<int>(m_count);
    const int start = -span / 2 + seg * static_cast<int>(m_selected);
    c.drawArc(cx, cy, r, -span / 2, span / 2, 3, Color::DarkGray);
    c.drawArc(cx, cy, r, static_cast<int16_t>(start), static_cast<int16_t>(start + seg), 3, Color::White);
}

}  // namespace ap
