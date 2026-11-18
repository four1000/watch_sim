#pragma once

#include <cstdint>
#include <string_view>

namespace ap {

enum class Color : uint8_t {
    Black, White, LightGray, Gray, DarkGray,
    Red, Orange, Yellow, Green, Blue, Cyan,
    // HR zone colours
    Zone1, Zone2, Zone3, Zone4, Zone5,
};

enum class Font : uint8_t { Small, Medium, Large, Number };
enum class Align : uint8_t { Left, Center, Right };

// Drawing surface the screens render to. The AP layer only knows this
// interface; the simulator adapts it to OpenCV, the watch to its display
// driver / graphics library (Adapter pattern).
//
// Coordinates are pixels on a square, round-masked display.
// Text: (x, y) is the anchor given by `align` horizontally and the *vertical
// centre* of the text, so layouts do not depend on font metrics.
class ICanvas {
public:
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;
    virtual void clear(Color c) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color c) = 0;
    virtual void fillCircle(int16_t cx, int16_t cy, int16_t r, Color c) = 0;
    virtual void drawArc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t endDeg,
                         uint8_t thickness, Color c) = 0;  // 0 deg = 3 o'clock, clockwise
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
        default: return Color::Gray;  // zone 0 = resting / below zone 1
    }
}

}  // namespace ap
