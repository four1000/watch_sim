#pragma once

#include <cstdint>
#include <string_view>

#include "ap/ui/canvas.h"

namespace ap {

enum class Theme : uint8_t { Dark, Light };

constexpr Color themed(Color c, Theme t) {
    if (t == Theme::Dark) return c;
    switch (c) {
        case Color::Black:     return Color::White;
        case Color::White:     return Color::Black;
        case Color::LightGray: return Color::DarkGray;
        case Color::DarkGray:  return Color::LightGray;
        case Color::Yellow:    return Color::Orange;
        case Color::Cyan:      return Color::Blue;
        case Color::Zone1:     return Color::Gray;
        case Color::Zone1Dim:  return themed(Color::Zone1, t);
        case Color::Zone2Dim:  return Color::Zone2;
        case Color::Zone3Dim:  return Color::Zone3;
        case Color::Zone4Dim:  return Color::Zone4;
        case Color::Zone5Dim:  return Color::Zone5;
        default:               return c;
    }
}

class ThemedCanvas final : public ICanvas {
public:
    ThemedCanvas(ICanvas& inner, Theme theme) : m_inner(inner), m_theme(theme) {}
    ThemedCanvas(const ThemedCanvas&) = delete;
    ThemedCanvas& operator=(const ThemedCanvas&) = delete;

    Theme theme() const { return m_theme; }

    int16_t width() const override { return m_inner.width(); }
    int16_t height() const override { return m_inner.height(); }
    void clear(Color c) override { m_inner.clear(map(c)); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color c) override { m_inner.fillRect(x, y, w, h, map(c)); }
    void fillCircle(int16_t cx, int16_t cy, int16_t r, Color c) override { m_inner.fillCircle(cx, cy, r, map(c)); }
    void drawArc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t endDeg, uint8_t thickness,
                 Color c) override {
        m_inner.drawArc(cx, cy, r, startDeg, endDeg, thickness, map(c));
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thickness, Color c) override {
        m_inner.drawLine(x0, y0, x1, y1, thickness, map(c));
    }
    void drawText(int16_t x, int16_t y, std::string_view text, Font f, Color c, Align a) override {
        m_inner.drawText(x, y, text, f, map(c), a);
    }

private:
    Color map(Color c) const { return themed(c, m_theme); }

    ICanvas& m_inner;
    Theme m_theme;
};
}
