#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/core/time.h"

namespace ap {

class ITimerListener {
public:
    virtual void onTimer(uint8_t tag, TickMs nowMs) = 0;

protected:
    ~ITimerListener() = default;
};

struct TimerHandle {
    uint8_t slot = 0xFF;
    uint8_t generation = 0;
    bool valid() const { return slot != 0xFF; }
};

class SoftTimerPool {
public:
    static constexpr std::size_t kCapacity = 8;

    TimerHandle start(TickMs nowMs, uint32_t delayMs, ITimerListener& listener, uint8_t tag);

    bool cancel(TimerHandle h);

    bool isRunning(TimerHandle h) const;

    void poll(TickMs nowMs);

    std::size_t active() const;

private:
    struct Slot {
        ITimerListener* listener = nullptr;
        TickMs deadline = 0;
        uint8_t tag = 0;
        uint8_t generation = 0;
        bool armed = false;
    };
    std::array<Slot, kCapacity> m_slots{};
    bool m_polling = false;
};
}
