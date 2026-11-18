#pragma once

#include <cstdint>

#include "ap/core/time.h"
#include "ap/services/sensor_types.h"

// ============================================================================
// Backend service interfaces, *owned by the AP layer* (Dependency Inversion).
//
// The AP layer declares what it needs; the platform provides it:
//   - on the watch:     adapters over the GPS / optical HR / flash drivers
//   - in the simulator: platform/sim/*  (threads, sliders, CSV files)
//   - in unit tests:    tests/fakes.h
//
// Interfaces are non-copyable and have protected non-virtual destructors:
// objects are owned by the composition root and never deleted through these
// pointers, so a vtable slot for the destructor would be wasted.
// ============================================================================

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

// ---- Sensors: control path only. Data arrives asynchronously through ISensorSink.
class IGpsService {
public:
    AP_INTERFACE(IGpsService)
    virtual bool start() = 0;  // false: hardware error, service stays off
    virtual void stop() = 0;   // safe when already stopped
};

class IHeartRateService {
public:
    AP_INTERFACE(IHeartRateService)
    virtual bool start() = 0;
    virtual void stop() = 0;
};

// Implemented by the AP layer (WatchApp). Sensor drivers / ISRs / sim threads
// call these from *their* context; implementations must be thread-safe and non-blocking.
class ISensorSink {
public:
    AP_INTERFACE(ISensorSink)
    virtual bool postHr(const HrSample& s) = 0;
    virtual bool postGps(const GpsSample& s) = 0;
};

// ---- Activity storage (FIT file on the watch, CSV in the simulator)
struct LapRecord {
    uint16_t index;       // 1-based
    uint32_t durationMs;
    uint32_t distanceDm;
    uint8_t avgHr;        // 0 = no HR during the lap
};

struct ActivitySummary {
    uint32_t elapsedMs;   // moving time, pauses excluded
    uint32_t distanceDm;
    uint16_t laps;
    uint8_t avgHr;
    uint8_t maxHr;
};

class IActivityRecorder {
public:
    AP_INTERFACE(IActivityRecorder)
    virtual bool begin(TickMs startMs) = 0;  // false: storage full / unavailable
    virtual bool recordLap(const LapRecord& lap) = 0;
    virtual bool finish(const ActivitySummary& summary) = 0;
    virtual void discard() = 0;
};

// ---- Outputs
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

// ---- Wall-clock time for the watch face (separate from the monotonic TickMs)
struct LocalTime {
    uint16_t year;
    uint8_t month;    // 1..12
    uint8_t day;      // 1..31
    uint8_t weekday;  // 0 = Sunday
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

}  // namespace ap
