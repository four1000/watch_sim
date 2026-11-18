#pragma once

#include <cstdint>
#include <string_view>

namespace ap {

enum class Color : uint8_t {
    Black, White, LightGray, Gray, DarkGray,
    Red, Orange, Yellow, Green, Blue, Cyan,
    Zone1, Zone2, Zone3, Zone4, Zone5,
    Zone1Dim, Zone2Dim, Zone3Dim, Zone4Dim, Zone5Dim,
};

enum class Font : uint8_t { Small, Medium, Large, Number };
enum class Align : uint8_t { Left, Center, Right };

class ICanvas {
public:
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;
    virtual void clear(Color c) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color c) = 0;
    virtual void fillCircle(int16_t cx, int16_t cy, int16_t r, Color c) = 0;
    virtual void drawArc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t endDeg,
                         uint8_t thickness, Color c) = 0;
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thickness, Color c) = 0;
    virtual void drawText(int16_t x, int16_t y, std::string_view text, Font f, Color c, Align a) = 0;

protected:
    ~ICanvas() = default;
};

inline Color zoneColor(uint8_t zone) {
    switch (zone) {
        case 1: return Color::Zone1;
        case 2: return Color::Zone2;
        case 3: return Color::Zone3;
        case 4: return Color::Zone4;
        case 5: return Color::Zone5;
        default: return Color::Gray;
    }
}

inline Color zoneColorDim(uint8_t zone) {
    switch (zone) {
        case 1: return Color::Zone1Dim;
        case 2: return Color::Zone2Dim;
        case 3: return Color::Zone3Dim;
        case 4: return Color::Zone4Dim;
        case 5: return Color::Zone5Dim;
        default: return Color::DarkGray;
    }
}

}
