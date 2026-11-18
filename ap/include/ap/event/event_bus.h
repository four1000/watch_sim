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

class IEventTrace {
public:
    virtual void onDispatch(const Event& e, std::size_t receivers) = 0;

protected:
    ~IEventTrace() = default;
};

class EventBus;

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

class EventBus {
public:
    static constexpr std::size_t kMaxSubscribers = 16;
    static constexpr std::size_t kQueueCapacity = 32;

    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    [[nodiscard]] Subscription subscribe(IEventListener& l, TopicMask mask);

    bool publish(const Event& e);

    std::size_t dispatch(std::size_t budget = kQueueCapacity * 2);

    std::size_t pending() const { return m_queue.size(); }
    std::size_t subscribers() const;
    uint16_t dropped() const { return m_dropped; }
    void setTrace(IEventTrace* t) { m_trace = t; }

private:
    friend class Subscription;
    void unsubscribe(uint8_t slot, uint8_t generation);

    struct Slot {
        IEventListener* listener = nullptr;
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

}
