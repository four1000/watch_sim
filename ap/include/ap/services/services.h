#pragma once

#include <cstdint>

#include "ap/core/time.h"
#include "ap/services/sensor_types.h"

namespace ap {

#define AP_INTERFACE(Name)                       \
    Name(const Name&) = delete;                  \
    Name& operator=(const Name&) = delete;       \
                                                 \
protected:                                       \
    Name() = default;                            \
    ~Name() = default;                           \
                                                 \
public:

class IGpsService {
public:
    AP_INTERFACE(IGpsService)
    virtual bool start() = 0;
    virtual void stop() = 0;
};

class IHeartRateService {
public:
    AP_INTERFACE(IHeartRateService)
    virtual bool start() = 0;
    virtual void stop() = 0;
};

class ISensorSink {
public:
    AP_INTERFACE(ISensorSink)
    virtual bool postHr(const HrSample& s) = 0;
    virtual bool postGps(const GpsSample& s) = 0;
};

struct LapRecord {
    uint16_t index;
    uint32_t durationMs;
    uint32_t distanceDm;
    uint8_t avgHr;
};

struct ActivitySummary {
    uint32_t elapsedMs;
    uint32_t distanceDm;
    uint16_t laps;
    uint8_t avgHr;
    uint8_t maxHr;
};

class IActivityRecorder {
public:
    AP_INTERFACE(IActivityRecorder)
    virtual bool begin(TickMs startMs) = 0;
    virtual bool recordLap(const LapRecord& lap) = 0;
    virtual bool finish(const ActivitySummary& summary) = 0;
    virtual void discard() = 0;
};

enum class VibePattern : uint8_t { Short, Double, Long };

class IVibrator {
public:
    AP_INTERFACE(IVibrator)
    virtual void vibrate(VibePattern p) = 0;
};

class IBacklight {
public:
    AP_INTERFACE(IBacklight)
    virtual void setOn(bool on) = 0;
};

struct LocalTime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

class IWallClock {
public:
    AP_INTERFACE(IWallClock)
    virtual LocalTime now() const = 0;
};

#undef AP_INTERFACE
}
