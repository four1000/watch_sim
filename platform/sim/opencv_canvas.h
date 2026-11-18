#pragma once

#include <opencv2/core.hpp>

#include "ap/ui/canvas.h"

namespace sim {

// Adapter: ap::ICanvas (what the AP layer wants) -> OpenCV drawing calls.
// The AP layer works in logical 240x240 pixels; `scale` renders at a higher
// resolution so the simulator window stays crisp.
class OpenCvCanvas final : public ap::ICanvas {
public:
    OpenCvCanvas(int16_t logicalSize, int scale);

    cv::Mat& image() { return m_img; }
    const cv::Mat& image() const { return m_img; }

    int16_t width() const override { return m_size; }
    int16_t height() const override { return m_size; }
    void clear(ap::Color c) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, ap::Color c) override;
    void fillCircle(int16_t cx, int16_t cy, int16_t r, ap::Color c) override;
    void drawArc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t endDeg, uint8_t thickness,
                 ap::Color c) override;
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thickness, ap::Color c) override;
    void drawText(int16_t x, int16_t y, std::string_view text, ap::Font f, ap::Color c, ap::Align a) override;

    static cv::Scalar toBgr(ap::Color c);

private:
    cv::Point pt(int x, int y) const { return {x * m_scale, y * m_scale}; }

    int16_t m_size;
    int m_scale;
    cv::Mat m_img;
};

}  // namespace sim
