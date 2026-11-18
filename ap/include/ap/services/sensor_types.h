#pragma once

#include <cstdint>

#include "ap/core/time.h"

namespace ap {

struct HrSample {
    TickMs timestampMs;
    uint8_t bpm;
    bool valid;
};

struct GpsSample {
    TickMs timestampMs;
    uint32_t odometerDm;
    uint16_t accuracyM;
    bool hasFix;
};
}
