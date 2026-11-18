#pragma once

#include <array>

#include <opencv2/core.hpp>

#include "ap/input/input_types.h"

namespace sim {

class WatchFrame {
public:
    static constexpr int kSize = 600;
    static constexpr int kDisplayPx = 480;

    WatchFrame();

    struct State {
        std::array<bool, ap::kKeyCount> pressed{};
        bool backlight = false;
        bool vibrating = false;
    };

    void draw(cv::Mat& frame, cv::Point origin, const cv::Mat& display, const State& s) const;

    bool hit(cv::Point origin, cv::Point p, ap::KeyId& key) const;

private:
    std::array<cv::Rect, ap::kKeyCount> m_buttons;
    cv::Mat m_mask;
};
}
