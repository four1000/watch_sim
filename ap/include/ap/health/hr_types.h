#pragma once

#include <cstdint>

namespace ap {

constexpr uint8_t kZoneCount = 5;

enum class HrAlertKind : uint8_t { AboveTarget, BelowTarget };

struct HrAlertInfo {
    HrAlertKind kind;
    uint8_t bpm;
    uint8_t zone;
    uint8_t targetZone;
};

struct HrZoneChange {
    uint8_t zone;
    uint8_t bpm;
};
}
