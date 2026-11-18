#pragma once

#include <cstddef>
#include <deque>
#include <string>

#include <opencv2/core.hpp>

#include "ap/event/event_bus.h"

namespace ap {
class WatchApp;
}

namespace sim {

struct SensorWorld;
class CsvActivityRecorder;

// Collects the last N dispatched events for the trace view. Called on the AP
// thread (inside EventBus::dispatch) - the same thread that draws the panel.
class EventTraceLog final : public ap::IEventTrace {
public:
    static constexpr std::size_t kLines = 9;
    void onDispatch(const ap::Event& e, std::size_t receivers) override;
    const std::deque<std::string>& lines() const { return m_lines; }
    bool showSamples = false;  // HR/GPS/tick events are frequent; hidden by default
    bool printToStdout = false;

private:
    std::deque<std::string> m_lines;
};

// Right-hand side of the window: sensor knobs, fault injection, AP state, trace.
class DevPanel {
public:
    static constexpr int kWidth = 400;

    DevPanel(SensorWorld& world, CsvActivityRecorder& recorder, EventTraceLog& trace)
        : m_world(world), m_recorder(recorder), m_trace(trace) {}

    void draw(cv::Mat& frame, int x, const ap::WatchApp& app, ap::TickMs now);

private:
    SensorWorld& m_world;
    CsvActivityRecorder& m_recorder;
    EventTraceLog& m_trace;
    int m_hr = 75;
    double m_speed = 0.0;
};

}  // namespace sim
