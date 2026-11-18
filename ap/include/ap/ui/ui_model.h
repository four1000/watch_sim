#pragma once

#include <cstddef>
#include <cstdint>

#include "ap/activity/activity_metrics.h"
#include "ap/app/settings.h"
#include "ap/services/services.h"

namespace ap {

// View model: a by-value snapshot of everything any screen may display.
// Built once per frame by WatchApp (the presenter) from the session, the zone
// monitor and the settings. Screens read it and never call a service, so:
//   - rendering cannot change state,
//   - screens are tested by filling a UiModel by hand,
//   - the model could be handed to a separate UI task without locking.
struct UiModel {
    TickMs nowMs = 0;
    LocalTime time{};
    ActivitySnapshot activity{};
    uint8_t hrBpm = 0;
    bool hrValid = false;
    uint8_t hrZone = 0;
    UserSettings settings{};
    bool keyLocked = false;
    bool backlightOn = false;
};

// Small formatting helpers shared by screens (no printf of floats, no heap).
// All write a NUL-terminated string into `buf` and return it.
const char* formatDuration(uint32_t ms, char* buf, std::size_t len);     // "4:05" / "1:02:03"
const char* formatDistanceKm(uint32_t dm, char* buf, std::size_t len);   // "3.42"
const char* formatPace(uint16_t secPerKm, char* buf, std::size_t len);   // "5:12" / "--:--"

}  // namespace ap
