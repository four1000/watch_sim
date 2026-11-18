#include "ap/event/event.h"

#include <cstdio>

namespace ap {

namespace {
Event base(EventType t, TickMs ts) {
    Event e{};
    e.type = t;
    e.timestampMs = ts;
    return e;
}
}

Event Event::makeGesture(const GestureEvent& g) {
    Event e = base(EventType::Gesture, g.timestampMs);
    e.gesture = g;
    return e;
}
Event Event::makeHr(const HrSample& s) {
    Event e = base(EventType::HrSample, s.timestampMs);
    e.hr = s;
    return e;
}
Event Event::makeGps(const GpsSample& s) {
    Event e = base(EventType::GpsSample, s.timestampMs);
    e.gps = s;
    return e;
}
Event Event::makeSecondTick(TickMs t) { return base(EventType::SecondTick, t); }
Event Event::makeActivity(TickMs t, ActivityState from, ActivityState to, ActivityOutcome o) {
    Event e = base(EventType::ActivityStateChanged, t);
    e.activity = ActivityStateChange{from, to, o};
    return e;
}
Event Event::makeLap(TickMs t, const LapRecord& lap) {
    Event e = base(EventType::LapCompleted, t);
    e.lap = lap;
    return e;
}
Event Event::makeZone(TickMs t, uint8_t zone, uint8_t bpm) {
    Event e = base(EventType::HrZoneChanged, t);
    e.zone = HrZoneChange{zone, bpm};
    return e;
}
Event Event::makeAlert(TickMs t, const HrAlertInfo& a) {
    Event e = base(EventType::HrAlert, t);
    e.alert = a;
    return e;
}
Event Event::makeSettingsChanged(TickMs t) { return base(EventType::SettingsChanged, t); }
Event Event::makeKeyLock(TickMs t, bool locked) {
    Event e = base(EventType::KeyLockChanged, t);
    e.keyLocked = locked;
    return e;
}

const char* toString(EventType t) {
    switch (t) {
        case EventType::Gesture:              return "Gesture";
        case EventType::HrSample:             return "HrSample";
        case EventType::GpsSample:            return "GpsSample";
        case EventType::SecondTick:           return "SecondTick";
        case EventType::ActivityStateChanged: return "ActivityState";
        case EventType::LapCompleted:         return "Lap";
        case EventType::HrZoneChanged:        return "HrZone";
        case EventType::HrAlert:              return "HrAlert";
        case EventType::SettingsChanged:      return "Settings";
        case EventType::KeyLockChanged:       return "KeyLock";
        case EventType::Count:                break;
    }
    return "?";
}

namespace {
const char* toString(ActivityOutcome o) {
    switch (o) {
        case ActivityOutcome::None:       return "";
        case ActivityOutcome::Saved:      return " (saved)";
        case ActivityOutcome::SaveFailed: return " (save failed)";
        case ActivityOutcome::Discarded:  return " (discarded)";
        case ActivityOutcome::Cancelled:  return " (cancelled)";
        case ActivityOutcome::StartFailed: return " (start failed)";
    }
    return "";
}
}

const char* describe(const Event& e, char* buf, std::size_t len) {
    if (buf == nullptr || len == 0) return "";
    const char* name = toString(e.type);
    switch (e.type) {
        case EventType::Gesture:
            std::snprintf(buf, len, "%s %s %s", name, toString(e.gesture.key), toString(e.gesture.gesture));
            break;
        case EventType::HrSample:
            if (e.hr.valid) std::snprintf(buf, len, "%s %u bpm", name, static_cast<unsigned>(e.hr.bpm));
            else            std::snprintf(buf, len, "%s --", name);
            break;
        case EventType::GpsSample:
            std::snprintf(buf, len, "%s %s odo=%lu dm acc=%u m", name, e.gps.hasFix ? "fix" : "nofix",
                          static_cast<unsigned long>(e.gps.odometerDm), static_cast<unsigned>(e.gps.accuracyM));
            break;
        case EventType::ActivityStateChanged:
            std::snprintf(buf, len, "%s %s -> %s%s", name, toString(e.activity.from),
                          toString(e.activity.to), toString(e.activity.outcome));
            break;
        case EventType::LapCompleted:
            std::snprintf(buf, len, "%s #%u %lu ms", name, static_cast<unsigned>(e.lap.index),
                          static_cast<unsigned long>(e.lap.durationMs));
            break;
        case EventType::HrZoneChanged:
            std::snprintf(buf, len, "%s Z%u (%u bpm)", name, static_cast<unsigned>(e.zone.zone),
                          static_cast<unsigned>(e.zone.bpm));
            break;
        case EventType::HrAlert:
            std::snprintf(buf, len, "%s %s Z%u target Z%u", name,
                          e.alert.kind == HrAlertKind::AboveTarget ? "HIGH" : "LOW",
                          static_cast<unsigned>(e.alert.zone), static_cast<unsigned>(e.alert.targetZone));
            break;
        case EventType::KeyLockChanged:
            std::snprintf(buf, len, "%s %s", name, e.keyLocked ? "locked" : "unlocked");
            break;
        case EventType::SecondTick:
        case EventType::SettingsChanged:
        case EventType::Count:
            std::snprintf(buf, len, "%s", name);
            break;
    }
    return buf;
}

}
