#include "sim/opencv_canvas.h"

#include <string>

#include <opencv2/imgproc.hpp>

namespace sim {
namespace {

struct FontSpec {
    int face;
    double scale;
    int thickness;
};

FontSpec spec(ap::Font f) {
    switch (f) {
        case ap::Font::Small:  return {cv::FONT_HERSHEY_SIMPLEX, 0.45, 1};
        case ap::Font::Medium: return {cv::FONT_HERSHEY_SIMPLEX, 0.70, 2};
        case ap::Font::Large:  return {cv::FONT_HERSHEY_DUPLEX, 0.95, 2};
        case ap::Font::Number: return {cv::FONT_HERSHEY_DUPLEX, 1.70, 3};
    }
    return {cv::FONT_HERSHEY_SIMPLEX, 0.5, 1};
}
}

OpenCvCanvas::OpenCvCanvas(int16_t logicalSize, int scale)
    : m_size(logicalSize), m_scale(scale), m_img(logicalSize * scale, logicalSize * scale, CV_8UC3) {}

cv::Scalar OpenCvCanvas::toBgr(ap::Color c) {
    switch (c) {
        case ap::Color::Black:     return {0, 0, 0};
        case ap::Color::White:     return {255, 255, 255};
        case ap::Color::LightGray: return {200, 200, 200};
        case ap::Color::Gray:      return {140, 140, 140};
        case ap::Color::DarkGray:  return {64, 64, 64};
        case ap::Color::Red:       return {60, 60, 220};
        case ap::Color::Orange:    return {0, 140, 255};
        case ap::Color::Yellow:    return {0, 215, 255};
        case ap::Color::Green:     return {90, 200, 80};
        case ap::Color::Blue:      return {190, 110, 30};
        case ap::Color::Cyan:      return {230, 200, 0};
        case ap::Color::Zone1:     return {170, 170, 170};
        case ap::Color::Zone2:     return {230, 150, 40};
        case ap::Color::Zone3:     return {70, 190, 60};
        case ap::Color::Zone4:     return {0, 150, 255};
        case ap::Color::Zone5:     return {60, 50, 230};
        case ap::Color::Zone1Dim:  return toBgr(ap::Color::Zone1) * 0.8;
        case ap::Color::Zone2Dim:  return toBgr(ap::Color::Zone2) * 0.8;
        case ap::Color::Zone3Dim:  return toBgr(ap::Color::Zone3) * 0.8;
        case ap::Color::Zone4Dim:  return toBgr(ap::Color::Zone4) * 0.8;
        case ap::Color::Zone5Dim:  return toBgr(ap::Color::Zone5) * 0.8;
    }
    return {255, 0, 255};
}

void OpenCvCanvas::clear(ap::Color c) { m_img.setTo(toBgr(c)); }

void OpenCvCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, ap::Color c) {
    cv::rectangle(m_img, cv::Rect(x * m_scale, y * m_scale, w * m_scale, h * m_scale), toBgr(c), cv::FILLED);
}

void OpenCvCanvas::fillCircle(int16_t cx, int16_t cy, int16_t r, ap::Color c) {
    cv::circle(m_img, pt(cx, cy), r * m_scale, toBgr(c), cv::FILLED, cv::LINE_AA);
}

void OpenCvCanvas::drawArc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t endDeg, uint8_t thickness,
                           ap::Color c) {
    cv::ellipse(m_img, pt(cx, cy), cv::Size(r * m_scale, r * m_scale), 0, startDeg, endDeg, toBgr(c),
                thickness * m_scale, cv::LINE_AA);
}

void OpenCvCanvas::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thickness, ap::Color c) {
    cv::line(m_img, pt(x0, y0), pt(x1, y1), toBgr(c), thickness * m_scale, cv::LINE_AA);
}

void OpenCvCanvas::drawText(int16_t x, int16_t y, std::string_view text, ap::Font f, ap::Color c, ap::Align a) {
    const FontSpec fs = spec(f);
    const std::string s(text);
    int baseline = 0;
    const double scale = fs.scale * m_scale;
    const int thick = fs.thickness * m_scale;
    const cv::Size sz = cv::getTextSize(s, fs.face, scale, thick, &baseline);
    int px = x * m_scale;
    if (a == ap::Align::Center) px -= sz.width / 2;
    else if (a == ap::Align::Right) px -= sz.width;
    const int py = y * m_scale + sz.height / 2;
    cv::putText(m_img, s, cv::Point(px, py), fs.face, scale, toBgr(c), thick, cv::LINE_AA);
}

}
