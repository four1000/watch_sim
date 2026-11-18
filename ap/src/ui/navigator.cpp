#include "ap/ui/navigator.h"

namespace ap {

bool Navigator::push(Screen& s, TickMs now) {
    if (m_depth == kMaxDepth || contains(s)) return false;
    if (Screen* t = top()) t->exit(now);
    m_stack[m_depth++] = &s;
    s.enter(now);
    return true;
}

bool Navigator::pop(TickMs now) {
    if (m_depth <= 1) return false;
    m_stack[m_depth - 1]->exit(now);
    m_stack[--m_depth] = nullptr;
    top()->enter(now);
    return true;
}

void Navigator::resetTo(Screen& root, TickMs now) {
    if (Screen* t = top()) t->exit(now);
    m_stack.fill(nullptr);
    m_stack[0] = &root;
    m_depth = 1;
    root.enter(now);
}

bool Navigator::popUntil(const Screen& s, TickMs now) {
    if (!contains(s)) return false;
    while (top() != &s) {
        if (!pop(now)) return false;
    }
    return true;
}

bool Navigator::contains(const Screen& s) const {
    for (std::size_t i = 0; i < m_depth; ++i) {
        if (m_stack[i] == &s) return true;
    }
    return false;
}

void Navigator::render(ICanvas& c, const UiModel& m) const {
    if (Screen* t = top()) t->render(c, m);
}

bool Navigator::handle(const GestureEvent& e) {
    Screen* t = top();
    if (t == nullptr) return false;
    if (t->handle(e)) return true;
    if (e.key == KeyId::Back && e.gesture == Gesture::Short) return pop(e.timestampMs);
    return false;
}

}
