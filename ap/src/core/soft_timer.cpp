#include "ap/core/soft_timer.h"

namespace ap {

TimerHandle SoftTimerPool::start(TickMs nowMs, uint32_t delayMs, ITimerListener& listener, uint8_t tag) {
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        if (s.listener != nullptr) continue;
        s.listener = &listener;
        s.deadline = static_cast<TickMs>(nowMs + delayMs);
        s.tag = tag;
        s.armed = !m_polling;
        return TimerHandle{static_cast<uint8_t>(i), s.generation};
    }
    return TimerHandle{};
}

bool SoftTimerPool::cancel(TimerHandle h) {
    if (!isRunning(h)) return false;
    Slot& s = m_slots[h.slot];
    s.listener = nullptr;
    ++s.generation;  // invalidates every copy of the handle
    return true;
}

bool SoftTimerPool::isRunning(TimerHandle h) const {
    if (h.slot >= m_slots.size()) return false;
    const Slot& s = m_slots[h.slot];
    return s.listener != nullptr && s.generation == h.generation;
}

void SoftTimerPool::poll(TickMs nowMs) {
    m_polling = true;
    for (Slot& s : m_slots) {
        if (s.listener == nullptr || !s.armed || !timeReached(nowMs, s.deadline)) continue;
        ITimerListener* l = s.listener;
        const uint8_t tag = s.tag;
        s.listener = nullptr;  // free the slot *before* the callback:
        ++s.generation;        // the listener may restart the same timer
        l->onTimer(tag, nowMs);
    }
    m_polling = false;
    for (Slot& s : m_slots) {
        if (s.listener != nullptr) s.armed = true;
    }
}

std::size_t SoftTimerPool::active() const {
    std::size_t n = 0;
    for (const Slot& s : m_slots) n += (s.listener != nullptr) ? 1u : 0u;
    return n;
}

}  // namespace ap
