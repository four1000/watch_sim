#pragma once

#include <cstdint>

namespace ap {

using TickMs = uint32_t;

constexpr TickMs elapsedMs(TickMs now, TickMs since) { return static_cast<TickMs>(now - since); }

constexpr bool timeReached(TickMs now, TickMs deadline) {
    return static_cast<int32_t>(static_cast<uint32_t>(now - deadline)) >= 0;
}

static_assert(timeReached(5u, 0xFFFFFFF0u), "wrap-safe compare");
static_assert(!timeReached(0xFFFFFFF0u, 5u), "wrap-safe compare");
static_assert(elapsedMs(5u, 0xFFFFFFFBu) == 10u, "wrap-safe elapsed");
}
