#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/core/time.h"

namespace ap {

class ITimerListener {
public:
    // Called from SoftTimerPool::poll() on the AP thread. `tag` is the value
    // given to start(), so one listener can own several timers.
    virtual void onTimer(uint8_t tag, TickMs nowMs) = 0;

protected:
    ~ITimerListener() = default;
};

// Handle = slot index + generation. A stale handle (timer already fired or
// cancelled, slot reused) never cancels somebody else's timer.
struct TimerHandle {
    uint8_t slot = 0xFF;
    uint8_t generation = 0;
    bool valid() const { return slot != 0xFF; }
};

// One-shot software timers polled from the AP main loop (no OS timer, no
// callback from another thread). The same idea as an RTOS soft-timer service.
class SoftTimerPool {
public:
    static constexpr std::size_t kCapacity = 8;

    // Returns an invalid handle when the pool is full or listener-less.
    TimerHandle start(TickMs nowMs, uint32_t delayMs, ITimerListener& listener, uint8_t tag);

    // Safe with stale/invalid handles. Returns true when a running timer was stopped.
    bool cancel(TimerHandle h);

    bool isRunning(TimerHandle h) const;

    // Fires every expired timer. A listener may start or cancel timers
    // from inside onTimer(); a timer started there fires on a later poll.
    void poll(TickMs nowMs);

    std::size_t active() const;

private:
    struct Slot {
        ITimerListener* listener = nullptr;  // nullptr = free
        TickMs deadline = 0;
        uint8_t tag = 0;
        uint8_t generation = 0;
        bool armed = false;  // false while being started inside poll()
    };
    std::array<Slot, kCapacity> m_slots{};
    bool m_polling = false;
};

}  // namespace ap
