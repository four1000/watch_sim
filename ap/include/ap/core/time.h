#pragma once

#include <cstdint>

namespace ap {

// The AP layer never reads a clock by itself: every time value is injected
// (event timestamps, step(nowMs)). That keeps logic deterministic and testable.
//
// Ticks are uint32_t milliseconds and wrap every ~49.7 days, so comparisons
// must use modular arithmetic, never `now >= deadline`.
using TickMs = uint32_t;

// Elapsed time from `since` to `now`, correct across a single wrap.
constexpr TickMs elapsedMs(TickMs now, TickMs since) { return static_cast<TickMs>(now - since); }

// True when `now` is at or after `deadline` (valid while |delta| < 2^31 ms).
constexpr bool timeReached(TickMs now, TickMs deadline) {
    return static_cast<int32_t>(static_cast<uint32_t>(now - deadline)) >= 0;
}

static_assert(timeReached(5u, 0xFFFFFFF0u), "wrap-safe compare");
static_assert(!timeReached(0xFFFFFFF0u, 5u), "wrap-safe compare");
static_assert(elapsedMs(5u, 0xFFFFFFFBu) == 10u, "wrap-safe elapsed");

}  // namespace ap
