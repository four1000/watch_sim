#define CVUI_IMPLEMENTATION
#include "cvui.h"

#include "sim/dev_panel.h"

#include <cstdio>
#include <iostream>

#include "ap/app/watch_app.h"
#include "sim/sensor_simulator.h"
#include "sim/sim_services.h"

namespace sim {

void EventTraceLog::onDispatch(const ap::Event& e, std::size_t receivers) {
    const bool sample = e.type == ap::EventType::HrSample || e.type == ap::EventType::GpsSample ||
                        e.type == ap::EventType::SecondTick;
    char what[64];
    ap::describe(e, what, sizeof what);
    char line[96];
    std::snprintf(line, sizeof line, "%7.2fs  %-36s ->%zu", (e.timestampMs % 10000000u) / 1000.0, what, receivers);
    if (printToStdout && !sample) std::cout << line << '\n';
    if (sample && !showSamples) return;
    m_lines.emplace_back(line);
    while (m_lines.size() > kLines) m_lines.pop_front();
}

void DevPanel::draw(cv::Mat& frame, int x, const ap::WatchApp& app, ap::TickMs now) {
    const unsigned text = 0xDDDDDD, dim = 0x999999, accent = 0x66CCFF;
    cv::rectangle(frame, cv::Rect(x, 0, kWidth, frame.rows), cv::Scalar(36, 34, 32), cv::FILLED);
    int y = 14;
    cvui::text(frame, x + 14, y, "AP LAYER DEV PANEL", 0.55, accent);
    y += 28;

    // ---- Sensors ---------------------------------------------------------------
    cvui::text(frame, x + 14, y, "Sensors (simulated drivers, 1 Hz thread)", 0.4, dim);
    y += 20;
    cvui::text(frame, x + 14, y + 16, "HR bpm", 0.42, text);
    // Read the world first, write back only on user change: scripts (and the
    // sensor thread) may set the world too, and the panel must not override them.
    m_hr = m_world.targetHr;
    if (cvui::trackbar(frame, x + 90, y, 290, &m_hr, 40, 200, 1, "%.0Lf", cvui::TRACKBAR_HIDE_SEGMENT_LABELS, 1)) {
        m_world.targetHr = m_hr;
    }
    y += 48;
    cvui::text(frame, x + 14, y + 16, "km/h", 0.42, text);
    m_speed = m_world.speedKmh;
    if (cvui::trackbar(frame, x + 90, y, 290, &m_speed, 0.0, 20.0, 1, "%.1Lf", cvui::TRACKBAR_HIDE_SEGMENT_LABELS, 0.5)) {
        m_world.speedKmh = m_speed;
    }
    y += 52;

    bool worn = m_world.hrWorn, signal = m_world.gpsSignal, gpsFault = m_world.gpsHardwareFault;
    bool failBegin = m_recorder.failBegin, failFinish = m_recorder.failFinish;
    cvui::checkbox(frame, x + 14, y, "HR sensor worn", &worn, text);
    cvui::checkbox(frame, x + 200, y, "GPS signal", &signal, text);
    y += 22;
    cvui::text(frame, x + 14, y + 2, "Fault injection:", 0.4, dim);
    y += 18;
    cvui::checkbox(frame, x + 14, y, "GPS start fails", &gpsFault, text);
    cvui::checkbox(frame, x + 200, y, "Storage full", &failBegin, text);
    y += 22;
    cvui::checkbox(frame, x + 14, y, "Save fails", &failFinish, text);
    cvui::checkbox(frame, x + 200, y, "Trace samples", &m_trace.showSamples, text);
    m_world.hrWorn = worn;
    m_world.gpsSignal = signal;
    m_world.gpsHardwareFault = gpsFault;
    m_recorder.failBegin = failBegin;
    m_recorder.failFinish = failFinish;
    y += 32;

    // ---- AP state ----------------------------------------------------------------
    cvui::text(frame, x + 14, y, "AP state", 0.4, dim);
    y += 18;
    const auto& snap = app.model().activity;
    cvui::printf(frame, x + 14, y, 0.45, text, "Activity : %s", ap::toString(app.activityState()));
    cvui::printf(frame, x + 200, y, 0.45, text, "GPS fix : %s", snap.gpsFix ? "yes" : "no");
    y += 18;
    cvui::printf(frame, x + 14, y, 0.45, text, "Screen   : %s", app.topScreenName());
    cvui::printf(frame, x + 200, y, 0.45, text, "Stack   : %zu", app.navigator().depth());
    y += 18;
    cvui::printf(frame, x + 14, y, 0.45, text, "Overlay  : %s", *app.overlayTitle() ? app.overlayTitle() : "-");
    cvui::printf(frame, x + 200, y, 0.45, text, "HR zone : %u", static_cast<unsigned>(app.model().hrZone));
    y += 18;
    cvui::printf(frame, x + 14, y, 0.45, text, "Inbox drop: %u", static_cast<unsigned>(app.inboxDropped()));
    cvui::printf(frame, x + 200, y, 0.45, text, "Bus drop: %u", static_cast<unsigned>(app.busDropped()));
    y += 18;
    cvui::printf(frame, x + 14, y, 0.45, text, "Tick     : 0x%08X", static_cast<unsigned>(now));
    y += 18;
    const std::string file = m_recorder.lastFile();
    if (!file.empty()) cvui::printf(frame, x + 14, y, 0.38, dim, "Saved: %s", file.c_str());
    y += 26;

    // ---- Event trace ------------------------------------------------------------
    cvui::text(frame, x + 14, y, "EventBus trace  (time  event  ->receivers)", 0.4, dim);
    y += 18;
    for (const auto& line : m_trace.lines()) {
        cvui::text(frame, x + 14, y, line, 0.38, 0xBBEEBB);
        y += 15;
    }

    // ---- Help -----------------------------------------------------------------------
    const int hy = frame.rows - 44;
    cvui::text(frame, x + 14, hy, "Keys: Space=LIGHT  Up/Down  Enter=START  Backspace=BACK", 0.38, dim);
    cvui::text(frame, x + 14, hy + 16, "or hold the buttons with the mouse.  Esc = quit", 0.38, dim);
}

}  // namespace sim
