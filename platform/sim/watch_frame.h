#pragma once

#include <array>

#include <opencv2/core.hpp>

#include "ap/input/input_types.h"

namespace sim {

// Draws the watch "hardware" around the AP-rendered display: case, bezel,
// the five buttons, backlight dimming and a vibration effect. Also owns the
// button hit areas used for mouse input. Pure presentation of the simulator,
// nothing here belongs to the AP layer.
class WatchFrame {
public:
    static constexpr int kSize = 600;          // square area in the window
    static constexpr int kDisplayPx = 480;     // 240 logical px x2

    WatchFrame();

    struct State {
        std::array<bool, ap::kKeyCount> pressed{};
        bool backlight = false;
        bool vibrating = false;
    };

    void draw(cv::Mat& frame, cv::Point origin, const cv::Mat& display, const State& s) const;

    // Which button (if any) is under `p` (window coordinates).
    bool hit(cv::Point origin, cv::Point p, ap::KeyId& key) const;

private:
    std::array<cv::Rect, ap::kKeyCount> m_buttons;  // relative to origin
    cv::Mat m_mask;                                  // round display mask
};

}  // namespace sim
