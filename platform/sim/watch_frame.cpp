#include "sim/watch_frame.h"

#include <cmath>

#include <opencv2/imgproc.hpp>

namespace sim {

namespace {
constexpr int kCenter = WatchFrame::kSize / 2;
constexpr int kCaseR = 268;
constexpr int kBezelR = 252;
constexpr int kDisplayR = WatchFrame::kDisplayPx / 2;

cv::Rect buttonAt(double clockDeg) {
    // clockDeg: 0 = 3 o'clock, clockwise (screen coordinates).
    const double rad = clockDeg * 3.14159265358979 / 180.0;
    const int cx = kCenter + static_cast<int>(std::cos(rad) * (kCaseR + 10));
    const int cy = kCenter + static_cast<int>(std::sin(rad) * (kCaseR + 10));
    return {cx - 32, cy - 15, 64, 30};
}

const char* label(ap::KeyId k) {
    switch (k) {
        case ap::KeyId::Light: return "LIGHT";
        case ap::KeyId::Up:    return "UP";
        case ap::KeyId::Down:  return "DOWN";
        case ap::KeyId::Start: return "START";
        case ap::KeyId::Back:  return "BACK";
        case ap::KeyId::Count: break;
    }
    return "";
}
}  // namespace

WatchFrame::WatchFrame() {
    m_buttons[ap::keyIndex(ap::KeyId::Light)] = buttonAt(215);
    m_buttons[ap::keyIndex(ap::KeyId::Up)] = buttonAt(180);
    m_buttons[ap::keyIndex(ap::KeyId::Down)] = buttonAt(145);
    m_buttons[ap::keyIndex(ap::KeyId::Start)] = buttonAt(-35);
    m_buttons[ap::keyIndex(ap::KeyId::Back)] = buttonAt(35);

    m_mask = cv::Mat::zeros(kDisplayPx, kDisplayPx, CV_8UC1);
    cv::circle(m_mask, {kDisplayR, kDisplayR}, kDisplayR, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
}

void WatchFrame::draw(cv::Mat& frame, cv::Point origin, const cv::Mat& display, const State& s) const {
    const cv::Point c = origin + cv::Point(kCenter, kCenter);
    // A vibrating watch shakes a little.
    const cv::Point shake = s.vibrating ? cv::Point((cv::getTickCount() / 200000) % 2 ? 3 : -3, 0) : cv::Point();

    for (std::size_t i = 0; i < m_buttons.size(); ++i) {
        const cv::Rect r = m_buttons[i] + origin + shake;
        const bool down = s.pressed[i];
        cv::rectangle(frame, r, down ? cv::Scalar(90, 200, 255) : cv::Scalar(110, 110, 120), cv::FILLED);
        cv::rectangle(frame, r, cv::Scalar(40, 40, 40), 1);
        const ap::KeyId k = static_cast<ap::KeyId>(i);
        int base = 0;
        const cv::Size ts = cv::getTextSize(label(k), cv::FONT_HERSHEY_SIMPLEX, 0.38, 1, &base);
        // Label on the part of the button that sticks out of the case.
        const bool left = (k == ap::KeyId::Light || k == ap::KeyId::Up || k == ap::KeyId::Down);
        const int tx = left ? r.x + 3 : r.x + r.width - ts.width - 3;
        cv::putText(frame, label(k), cv::Point(tx, r.y + r.height / 2 + ts.height / 2), cv::FONT_HERSHEY_SIMPLEX, 0.38,
                    down ? cv::Scalar(20, 20, 20) : cv::Scalar(235, 235, 235), 1, cv::LINE_AA);
    }

    cv::circle(frame, c + shake, kCaseR, cv::Scalar(50, 50, 56), cv::FILLED, cv::LINE_AA);
    cv::circle(frame, c + shake, kBezelR, cv::Scalar(25, 25, 28), cv::FILLED, cv::LINE_AA);
    if (s.vibrating) cv::circle(frame, c + shake, kCaseR, cv::Scalar(60, 60, 230), 4, cv::LINE_AA);

    cv::Mat shown;
    if (s.backlight) {
        display.convertTo(shown, -1, 1.0, 25.0);  // backlight: brighter, lifted blacks
    } else {
        display.convertTo(shown, -1, 0.78, 0.0);  // transflective display without light
    }
    const cv::Rect roi(c.x - kDisplayR + shake.x, c.y - kDisplayR + shake.y, kDisplayPx, kDisplayPx);
    shown.copyTo(frame(roi), m_mask);
}

bool WatchFrame::hit(cv::Point origin, cv::Point p, ap::KeyId& key) const {
    for (std::size_t i = 0; i < m_buttons.size(); ++i) {
        if ((m_buttons[i] + origin).contains(p)) {
            key = static_cast<ap::KeyId>(i);
            return true;
        }
    }
    return false;
}

}  // namespace sim
