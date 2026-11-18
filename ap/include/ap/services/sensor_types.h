#pragma once

#include <cstdint>

#include "ap/core/time.h"

namespace ap {

// Data the AP layer needs from the sensor services - not raw chip registers.
// PODs without default member initializers so they can live in Event's union.

struct HrSample {
    TickMs timestampMs;
    uint8_t bpm;     // valid only when `valid` is true, then 30..220
    bool valid;      // false: no reading (watch not worn, sensor lost)
};

struct GpsSample {
    TickMs timestampMs;
    // Distance accumulated by the GPS service since boot, in decimetres.
    // Not reset by start(); wraps modulo 2^32 (same contract as a hardware odometer).
    uint32_t odometerDm;
    uint16_t accuracyM;  // horizontal accuracy estimate, smaller is better
    bool hasFix;
};

}  // namespace ap
