#include "ap/event/event_bus.h"

#include <cstdint>

namespace ap {

// ---- Subscription ---------------------------------------------------------------

Subscription::Subscription(Subscription&& o) noexcept
    : m_bus(o.m_bus), m_slot(o.m_slot), m_generation(o.m_generation) {
    o.m_bus = nullptr;
}

Subscription& Subscription::operator=(Subscription&& o) noexcept {
    if (this != &o) {
        reset();
        m_bus = o.m_bus;
        m_slot = o.m_slot;
        m_generation = o.m_generation;
        o.m_bus = nullptr;
    }
    return *this;
}

void Subscription::reset() {
    if (m_bus != nullptr) {
        m_bus->unsubscribe(m_slot, m_generation);
        m_bus = nullptr;
    }
}

// ---- EventBus ---------------------------------------------------------------------

Subscription EventBus::subscribe(IEventListener& l, TopicMask mask) {
    if (mask == 0) return Subscription{};
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        if (s.listener != nullptr) continue;
        s.listener = &l;
        s.mask = mask;
        s.armed = !m_dispatching;
        return Subscription{this, static_cast<uint8_t>(i), s.generation};
    }
    return Subscription{};
}

void EventBus::unsubscribe(uint8_t slot, uint8_t generation) {
    if (slot >= m_slots.size()) return;
    Slot& s = m_slots[slot];
    if (s.generation != generation || s.listener == nullptr) return;
    // Tombstone: safe even while dispatch() is iterating, because dispatch
    // re-reads `listener` for every slot and skips nullptr.
    s.listener = nullptr;
    s.mask = 0;
    s.armed = false;
    ++s.generation;
}

bool EventBus::publish(const Event& e) {
    if (m_queue.push(e)) return true;
    if (m_dropped != UINT16_MAX) ++m_dropped;
    return false;
}

std::size_t EventBus::dispatch(std::size_t budget) {
    if (m_dispatching) return 0;  // re-entrant call from a listener: ignore
    m_dispatching = true;
    std::size_t delivered = 0;
    while (delivered < budget) {
        const auto e = m_queue.pop();
        if (!e) break;
        const TopicMask bit = topic(e->type);
        std::size_t receivers = 0;
        for (Slot& s : m_slots) {
            if (s.listener != nullptr && s.armed && (s.mask & bit) != 0) {
                s.listener->onEvent(*e);
                ++receivers;
            }
        }
        if (m_trace != nullptr) m_trace->onDispatch(*e, receivers);
        // Listeners subscribed while this event was delivered start now.
        for (Slot& s : m_slots) {
            if (s.listener != nullptr) s.armed = true;
        }
        ++delivered;
    }
    m_dispatching = false;
    return delivered;
}

std::size_t EventBus::subscribers() const {
    std::size_t n = 0;
    for (const Slot& s : m_slots) n += (s.listener != nullptr) ? 1u : 0u;
    return n;
}

}  // namespace ap
