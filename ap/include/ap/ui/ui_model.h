#pragma once

#include <cstddef>
#include <cstdint>

#include "ap/activity/activity_metrics.h"
#include "ap/app/settings.h"
#include "ap/services/services.h"

namespace ap {

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

const char* formatDuration(uint32_t ms, char* buf, std::size_t len);
const char* formatDistanceKm(uint32_t dm, char* buf, std::size_t len);
const char* formatPace(uint16_t secPerKm, char* buf, std::size_t len);
const char* formatDayMonthTime(const LocalTime& t, char* buf, std::size_t len);
const char* monthAbbrev(uint8_t month);
uint16_t paceSecPerKm(uint32_t durationMs, uint32_t distanceDm);
}
