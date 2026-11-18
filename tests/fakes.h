#pragma once

// Test doubles for every service the AP layer depends on.
// Because the AP layer owns these interfaces, the whole application can run
// in a unit test: no window, no thread, no clock.

#include <string>
#include <vector>

#include "ap/services/osal.h"
#include "ap/services/services.h"
#include "ap/ui/canvas.h"

namespace fakes {

class FakeGps final : public ap::IGpsService {
public:
    bool start() override { ++starts; running = startOk; return startOk; }
    void stop() override { ++stops; running = false; }
    bool startOk = true;
    bool running = false;
    int starts = 0;
    int stops = 0;
};

class FakeHr final : public ap::IHeartRateService {
public:
    bool start() override { running = true; return true; }
    void stop() override { running = false; }
    bool running = false;
};

class FakeRecorder final : public ap::IActivityRecorder {
public:
    bool begin(ap::TickMs t) override { ++begins; beganAt = t; return beginOk; }
    bool recordLap(const ap::LapRecord& lap) override { laps.push_back(lap); return true; }
    bool finish(const ap::ActivitySummary& s) override { ++finishes; summary = s; return finishOk; }
    void discard() override { ++discards; }

    bool beginOk = true;
    bool finishOk = true;
    int begins = 0, finishes = 0, discards = 0;
    ap::TickMs beganAt = 0;
    std::vector<ap::LapRecord> laps;
    ap::ActivitySummary summary{};
};

class FakeVibrator final : public ap::IVibrator {
public:
    void vibrate(ap::VibePattern p) override { patterns.push_back(p); }
    std::vector<ap::VibePattern> patterns;
};

class FakeBacklight final : public ap::IBacklight {
public:
    void setOn(bool o) override { on = o; ++calls; }
    bool on = false;
    int calls = 0;
};

class FakeClock final : public ap::IWallClock {
public:
    ap::LocalTime now() const override { return time; }
    ap::LocalTime time{2026, 11, 18, 3, 8, 42, 0};
};

class NullMutex final : public ap::osal::IMutex {
public:
    void lock() override { ++locks; }
    void unlock() override { ++unlocks; }
    int locks = 0, unlocks = 0;
};

// Records draw calls so tests can assert *what* a screen shows,
// independent of any graphics library.
class FakeCanvas final : public ap::ICanvas {
public:
    int16_t width() const override { return 240; }
    int16_t height() const override { return 240; }
    void clear(ap::Color c) override {
        texts.clear();
        textColors.clear();
        background = c;
        ++clears;
    }
    void fillRect(int16_t, int16_t, int16_t, int16_t, ap::Color) override {}
    void fillCircle(int16_t, int16_t, int16_t, ap::Color) override {}
    void drawArc(int16_t, int16_t, int16_t, int16_t, int16_t, uint8_t, ap::Color) override {}
    void drawLine(int16_t, int16_t, int16_t, int16_t, uint8_t, ap::Color) override {}
    void drawText(int16_t, int16_t, std::string_view t, ap::Font, ap::Color c, ap::Align) override {
        texts.emplace_back(t);
        textColors.push_back(c);
    }

    bool shows(const std::string& s) const {
        for (const auto& t : texts) {
            if (t.find(s) != std::string::npos) return true;
        }
        return false;
    }
    // Colour of the first text containing `s`; Magenta-like sentinel (Zone5) if absent.
    ap::Color colorOf(const std::string& s) const {
        for (std::size_t i = 0; i < texts.size(); ++i) {
            if (texts[i].find(s) != std::string::npos) return textColors[i];
        }
        return ap::Color::Zone5;
    }
    std::vector<std::string> texts;
    std::vector<ap::Color> textColors;
    ap::Color background = ap::Color::Zone5;
    int clears = 0;
};

}  // namespace fakes
