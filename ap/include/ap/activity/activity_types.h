#pragma once

#include <cstdint>

namespace ap {

enum class ActivityState : uint8_t {
    Idle,     // no activity
    Ready,    // activity chosen, GPS acquiring, timer not started
    Running,  // timer running, samples recorded
    Paused,   // timer stopped, waiting for Resume / Save / Discard
};

// Extra information attached to a state change, so the UI can show the right
// toast without knowing anything about GPS or the recorder.
// StartFailed / SaveFailed are published as a *self* transition (from == to).
enum class ActivityOutcome : uint8_t { None, Saved, SaveFailed, Discarded, Cancelled, StartFailed };

const char* toString(ActivityState s);

}  // namespace ap
