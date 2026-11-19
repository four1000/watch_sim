#pragma once

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

class FakeRecorder final : public ap::IActivityRecorder, public ap::IActivityHistory {
public:
    bool begin(ap::TickMs t) override {
        ++begins;
        beganAt = t;
        m_current.clear();
        return beginOk;
    }
    bool recordLap(const ap::LapRecord& lap) override {
        laps.push_back(lap);
        m_current.push_back(lap);
        return true;
    }
    bool finish(const ap::ActivitySummary& s) override {
        ++finishes;
        summary = s;
        if (finishOk) addToHistory(ap::ActivityRecord{startTime, s}, m_current);
        return finishOk;
    }
    void discard() override { ++discards; }

    std::size_t activityCount() const override { return m_history.size(); }
    bool activityAt(std::size_t i, ap::ActivityRecord& out) const override {
        if (i >= m_history.size()) return false;
        out = m_history[m_history.size() - 1 - i].record;
        return true;
    }
    bool lapAt(std::size_t i, std::size_t lap, ap::LapRecord& out) const override {
        if (i >= m_history.size()) return false;
        const auto& stored = m_history[m_history.size() - 1 - i];
        if (lap >= stored.laps.size()) return false;
        out = stored.laps[lap];
        return true;
    }

    void addToHistory(const ap::ActivityRecord& record, const std::vector<ap::LapRecord>& recordLaps) {
        m_history.push_back(Stored{record, recordLaps});
    }

    bool beginOk = true;
    bool finishOk = true;
    int begins = 0, finishes = 0, discards = 0;
    ap::TickMs beganAt = 0;
    std::vector<ap::LapRecord> laps;
    ap::ActivitySummary summary{};
    ap::LocalTime startTime{2026, 11, 18, 3, 8, 42, 0};

private:
    struct Stored {
        ap::ActivityRecord record;
        std::vector<ap::LapRecord> laps;
    };
    std::vector<ap::LapRecord> m_current;
    std::vector<Stored> m_history;
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

class FakeCanvas final : public ap::ICanvas {
public:
    int16_t width() const override { return 240; }
    int16_t height() const override { return 240; }
    void clear(ap::Color c) override {
        texts.clear();
        textColors.clear();
        arcs.clear();
        background = c;
        ++clears;
    }
    void fillRect(int16_t, int16_t, int16_t, int16_t, ap::Color) override {}
    void fillCircle(int16_t, int16_t, int16_t, ap::Color) override {}
    void drawArc(int16_t, int16_t, int16_t, int16_t, int16_t, uint8_t thickness, ap::Color c) override {
        arcs.push_back(Arc{thickness, c});
    }
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
    bool showsExactly(const std::string& s) const {
        for (const auto& t : texts) {
            if (t == s) return true;
        }
        return false;
    }
    ap::Color colorOf(const std::string& s) const {
        for (std::size_t i = 0; i < texts.size(); ++i) {
            if (texts[i].find(s) != std::string::npos) return textColors[i];
        }
        return ap::Color::Zone5;
    }
    struct Arc {
        uint8_t thickness;
        ap::Color color;
    };
    const Arc* arcIn(ap::Color c) const {
        for (const auto& a : arcs) {
            if (a.color == c) return &a;
        }
        return nullptr;
    }
    std::vector<std::string> texts;
    std::vector<ap::Color> textColors;
    std::vector<Arc> arcs;
    ap::Color background = ap::Color::Zone5;
    int clears = 0;
};
}
