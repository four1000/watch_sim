#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/core/ring_buffer.h"
#include "ap/event/event.h"

namespace ap {

class IEventListener {
public:
    virtual void onEvent(const Event& e) = 0;

protected:
    ~IEventListener() = default;
};

// Observes every dispatched event (dev-panel trace, logging). Optional.
class IEventTrace {
public:
    virtual void onDispatch(const Event& e, std::size_t receivers) = 0;

protected:
    ~IEventTrace() = default;
};

class EventBus;

// RAII subscription token (move-only). Destroying it unsubscribes, so a
// listener can never be called after it is gone - as long as the token is a
// member of the listener (declared after anything onEvent() uses).
class Subscription {
public:
    Subscription() = default;
    ~Subscription() { reset(); }
    Subscription(Subscription&& o) noexcept;
    Subscription& operator=(Subscription&& o) noexcept;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    void reset();
    bool active() const { return m_bus != nullptr; }

private:
    friend class EventBus;
    Subscription(EventBus* bus, uint8_t slot, uint8_t generation)
        : m_bus(bus), m_slot(slot), m_generation(generation) {}

    EventBus* m_bus = nullptr;
    uint8_t m_slot = 0;
    uint8_t m_generation = 0;
};

// Publish/Subscribe with *deferred* dispatch:
//   publish()  only enqueues (safe from inside any onEvent())
//   dispatch() delivers queued events in FIFO order, one at a time
//              (run-to-completion: a listener never re-enters itself)
//
// Fixed capacity for both subscribers and the queue; overflow is counted,
// never silently grown.
class EventBus {
public:
    static constexpr std::size_t kMaxSubscribers = 16;
    static constexpr std::size_t kQueueCapacity = 32;

    // Contract: the bus outlives every Subscription (WatchApp declares it first,
    // so it is destroyed last).
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // Returns an inactive Subscription when the table is full or mask == 0.
    // A listener subscribed during dispatch() starts with the *next* event.
    [[nodiscard]] Subscription subscribe(IEventListener& l, TopicMask mask);

    bool publish(const Event& e);  // false = queue full (counted in dropped())

    // Delivers up to `budget` events (including ones published meanwhile),
    // so a publish loop between two listeners cannot starve the main loop.
    std::size_t dispatch(std::size_t budget = kQueueCapacity * 2);

    std::size_t pending() const { return m_queue.size(); }
    std::size_t subscribers() const;
    uint16_t dropped() const { return m_dropped; }
    void setTrace(IEventTrace* t) { m_trace = t; }

private:
    friend class Subscription;
    void unsubscribe(uint8_t slot, uint8_t generation);

    struct Slot {
        IEventListener* listener = nullptr;  // nullptr = free (or tombstoned)
        TopicMask mask = 0;
        uint8_t generation = 0;
        bool armed = false;
    };

    std::array<Slot, kMaxSubscribers> m_slots{};
    RingBuffer<Event, kQueueCapacity> m_queue;
    uint16_t m_dropped = 0;
    bool m_dispatching = false;
    IEventTrace* m_trace = nullptr;
};

}  // namespace ap
