#pragma once

#include <cstdint>

namespace ap {

enum class ActivityState : uint8_t {
    Idle,
    Ready,
    Running,
    Paused,
};

enum class ActivityOutcome : uint8_t { None, Saved, SaveFailed, Discarded, Cancelled, StartFailed };

const char* toString(ActivityState s);

}
