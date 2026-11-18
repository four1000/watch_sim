#pragma once

#include <cstdint>

namespace ap {

// Zone 0 = below zone 1 (or no reading). Zones 1..5 by % of max HR:
//   Z1 50-60%  Z2 60-70%  Z3 70-80%  Z4 80-90%  Z5 >= 90%
constexpr uint8_t kZoneCount = 5;

enum class HrAlertKind : uint8_t { AboveTarget, BelowTarget };

struct HrAlertInfo {
    HrAlertKind kind;
    uint8_t bpm;
    uint8_t zone;
    uint8_t targetZone;
};

struct HrZoneChange {
    uint8_t zone;  // 0..5
    uint8_t bpm;   // 0 = no reading
};

}  // namespace ap
