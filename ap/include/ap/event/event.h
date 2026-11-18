#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "ap/activity/activity_types.h"
#include "ap/health/hr_types.h"
#include "ap/input/input_types.h"
#include "ap/services/sensor_types.h"
#include "ap/services/services.h"

namespace ap {

enum class EventType : uint8_t {
    Gesture,               // input  -> router
    HrSample,              // sensor -> metrics, zone monitor, UI
    GpsSample,             // sensor -> metrics, UI
    SecondTick,            // clock  -> metrics, UI refresh
    ActivityStateChanged,  // session -> flow coordinator, zone monitor, UI
    LapCompleted,          // session -> overlay (toast + vibrate)
    HrZoneChanged,         // zone monitor -> UI
    HrAlert,               // zone monitor -> overlay
    SettingsChanged,       // settings -> zone monitor
    KeyLockChanged,        // key lock -> overlay
    Count
};
static_assert(static_cast<std::size_t>(EventType::Count) <= 32, "topic mask is 32 bits");

using TopicMask = uint32_t;
constexpr TopicMask topic(EventType t) { return TopicMask{1} << static_cast<uint32_t>(t); }
template <typename... Ts>
constexpr TopicMask topics(Ts... ts) { return (topic(ts) | ...); }

struct ActivityStateChange {
    ActivityState from;
    ActivityState to;
    ActivityOutcome outcome;
};

// Tagged union: fixed size (no heap, no std::function, no RTTI) so events
// can sit in a fixed-capacity queue and be copied with memcpy.
// Use the make* factories; read only the member matching `type`.
struct Event {
    EventType type;
    TickMs timestampMs;
    union {
        GestureEvent gesture;
        HrSample hr;
        GpsSample gps;
        ActivityStateChange activity;
        LapRecord lap;
        HrZoneChange zone;
        HrAlertInfo alert;
        bool keyLocked;
    };

    static Event makeGesture(const GestureEvent& g);
    static Event makeHr(const HrSample& s);
    static Event makeGps(const GpsSample& s);
    static Event makeSecondTick(TickMs t);
    static Event makeActivity(TickMs t, ActivityState from, ActivityState to, ActivityOutcome o);
    static Event makeLap(TickMs t, const LapRecord& lap);
    static Event makeZone(TickMs t, uint8_t zone, uint8_t bpm);
    static Event makeAlert(TickMs t, const HrAlertInfo& a);
    static Event makeSettingsChanged(TickMs t);
    static Event makeKeyLock(TickMs t, bool locked);
};
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(sizeof(Event) <= 24, "keep events small: the queue is statically allocated");

const char* toString(EventType t);

// Human-readable one-liner (dev panel trace / logs). Returns `buf`.
const char* describe(const Event& e, char* buf, std::size_t len);

}  // namespace ap
