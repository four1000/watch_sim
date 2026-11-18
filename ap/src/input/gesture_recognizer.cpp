#include "ap/input/gesture_recognizer.h"

#include <cstdint>

namespace ap {

const char* toString(KeyId k) {
    switch (k) {
        case KeyId::Light: return "LIGHT";
        case KeyId::Up:    return "UP";
        case KeyId::Down:  return "DOWN";
        case KeyId::Start: return "START";
        case KeyId::Back:  return "BACK";
        case KeyId::Count: break;
    }
    return "?";
}

const char* toString(Gesture g) {
    switch (g) {
        case Gesture::Short:  return "short";
        case Gesture::Long:   return "long";
        case Gesture::Double: return "double";
    }
    return "?";
}

GestureRecognizer::GestureRecognizer(const GestureConfig& cfg) : m_cfg(cfg) {}

void GestureRecognizer::setDoubleEnabled(KeyId key, bool enabled) {
    if (isValid(key)) m_keys[keyIndex(key)].doubleEnabled = enabled;
}

bool GestureRecognizer::onKeyEvent(const KeyEvent& e) {
    if (!isValid(e.key) || (e.edge != KeyEdge::Down && e.edge != KeyEdge::Up)) return false;

    // Let time-based transitions happen first, so an edge that arrives late
    // (after the window / threshold) is judged against the right state.
    poll(e.timestampMs);

    KeyCtx& k = m_keys[keyIndex(e.key)];
    const bool down = (e.edge == KeyEdge::Down);

    switch (k.state) {
        case KeyState::Idle:
            if (down) { k.state = KeyState::Pressed; k.since = k.pressedAt = e.timestampMs; }
            break;
        case KeyState::Pressed:
            if (!down) {
                if (k.doubleEnabled) {
                    k.state = KeyState::WaitSecond;
                    k.since = e.timestampMs;
                } else {
                    emit(e.key, Gesture::Short, k.pressedAt);
                    k.state = KeyState::Idle;
                }
            }
            break;
        case KeyState::LongFired:
            if (!down) k.state = KeyState::Idle;
            break;
        case KeyState::WaitSecond:
            if (down) { k.state = KeyState::SecondDown; k.since = e.timestampMs; }
            break;
        case KeyState::SecondDown:
            if (!down) {
                emit(e.key, Gesture::Double, k.pressedAt);
                k.state = KeyState::Idle;
            }
            break;
    }
    return true;
}

void GestureRecognizer::poll(TickMs nowMs) {
    for (std::size_t i = 0; i < m_keys.size(); ++i) {
        KeyCtx& k = m_keys[i];
        const KeyId key = static_cast<KeyId>(i);
        const uint32_t held = elapsedMs(nowMs, k.since);
        switch (k.state) {
            case KeyState::Pressed:
            case KeyState::SecondDown:
                // Holding the second press long turns it into a Long press;
                // the first tap is reported as a Short first (it did happen).
                if (held >= m_cfg.longPressMs) {
                    if (k.state == KeyState::SecondDown) {
                        emit(key, Gesture::Short, k.pressedAt);
                    }
                    emit(key, Gesture::Long, k.since);
                    k.state = KeyState::LongFired;
                }
                break;
            case KeyState::WaitSecond:
                if (held >= m_cfg.doubleWindowMs) {
                    emit(key, Gesture::Short, k.pressedAt);
                    k.state = KeyState::Idle;
                }
                break;
            case KeyState::Idle:
            case KeyState::LongFired:
                break;
        }
    }
}

void GestureRecognizer::reset() {
    for (KeyCtx& k : m_keys) {
        k.state = KeyState::Idle;
        k.since = k.pressedAt = 0;
    }
    m_out.clear();
    m_dropped = 0;
}

void GestureRecognizer::emit(KeyId key, Gesture g, TickMs t) {
    if (!m_out.push(GestureEvent{key, g, t})) {
        if (m_dropped != UINT16_MAX) ++m_dropped;
    }
}

}  // namespace ap
