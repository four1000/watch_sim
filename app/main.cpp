
#if defined(_DEBUG)
#pragma message("watch_sim: build RelWithDebInfo/Release - third_party ships only the release opencv_world DLL")
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

#include "ap/app/watch_app.h"
#include "cvui.h"
#include "sim/dev_panel.h"
#include "sim/opencv_canvas.h"
#include "sim/sensor_simulator.h"
#include "sim/sim_services.h"
#include "sim/watch_frame.h"

namespace {

constexpr const char* kWindow = "Fitness Watch Simulator - AP layer";
constexpr int kWatchMargin = 24;
const cv::Point kWatchOrigin{kWatchMargin, 0};
constexpr int kPanelX = sim::WatchFrame::kSize + 2 * kWatchMargin;
constexpr int kFrameW = kPanelX + sim::DevPanel::kWidth;
constexpr int kFrameH = sim::WatchFrame::kSize;
constexpr uint32_t kOdometerStart = 0xFFFF0000u;
constexpr uint32_t kWrapStartTick = 0xFFFFFFFFu - 30000u;

struct Options {
    std::string script;
    std::string outDir = "shots";
    bool wrap = false;
    bool trace = false;
};

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) o.script = argv[++i];
        else if (a == "--out" && i + 1 < argc) o.outDir = argv[++i];
        else if (a == "--wrap") o.wrap = true;
        else if (a == "--trace") o.trace = true;
    }
    return o;
}

bool parseKey(const std::string& s, ap::KeyId& k) {
    if (s == "LIGHT") k = ap::KeyId::Light;
    else if (s == "UP") k = ap::KeyId::Up;
    else if (s == "DOWN") k = ap::KeyId::Down;
    else if (s == "START") k = ap::KeyId::Start;
    else if (s == "BACK") k = ap::KeyId::Back;
    else return false;
    return true;
}

uint32_t vibeDurationMs(ap::VibePattern p) {
    switch (p) {
        case ap::VibePattern::Short:  return 150;
        case ap::VibePattern::Double: return 400;
        case ap::VibePattern::Long:   return 700;
    }
    return 0;
}

using KeyLevels = std::array<bool, ap::kKeyCount>;

struct Simulator {
    explicit Simulator(uint32_t odometerStart) : sensors(world, odometerStart) {}

    sim::SensorWorld world;
    sim::SensorSimulator sensors;
    sim::StdMutex inboxMutex;
    sim::SimVibrator vibrator;
    sim::SimBacklight backlight;
    std::unique_ptr<sim::SimWallClock> clock;
    std::unique_ptr<sim::CsvActivityRecorder> recorder;
    std::unique_ptr<ap::WatchApp> app;

    sim::OpenCvCanvas canvas{240, 2};
    sim::WatchFrame watch;
    sim::EventTraceLog trace;
    std::unique_ptr<sim::DevPanel> panel;
    cv::Mat frame = cv::Mat(kFrameH, kFrameW, CV_8UC3);

    uint32_t vibeSeen = 0;
    ap::TickMs vibeUntil = 0;
    KeyLevels keyLevel{};

    void build(std::unique_ptr<sim::SimWallClock> wallClock, bool loadHistory) {
        clock = std::move(wallClock);
        recorder = std::make_unique<sim::CsvActivityRecorder>("activities", *clock);
        if (loadHistory) recorder->loadExisting();
        app = std::make_unique<ap::WatchApp>(ap::WatchApp::Services{
            sensors.gps(), sensors.hr(), *recorder, *recorder, vibrator, backlight, *clock, inboxMutex});
        sensors.connect(*app);
        app->setTrace(&trace);
        panel = std::make_unique<sim::DevPanel>(world, *recorder, trace);
    }

    void setKeys(const KeyLevels& levels, ap::TickMs now) {
        for (std::size_t i = 0; i < levels.size(); ++i) {
            if (levels[i] == keyLevel[i]) continue;
            keyLevel[i] = levels[i];
            app->postKey(ap::KeyEvent{static_cast<ap::KeyId>(i), levels[i] ? ap::KeyEdge::Down : ap::KeyEdge::Up, now});
        }
    }

    void tick(ap::TickMs now) {
        app->step(now);
        if (app->needsRender()) app->render(canvas);
        if (vibrator.count() != vibeSeen) {
            vibeSeen = vibrator.count();
            vibeUntil = now + vibeDurationMs(vibrator.last());
        }
    }

    void compose(ap::TickMs now) {
        frame.setTo(cv::Scalar(24, 22, 20));
        sim::WatchFrame::State st;
        st.pressed = keyLevel;
        st.backlight = backlight.on();
        st.vibrating = !ap::timeReached(now, vibeUntil);
        watch.draw(frame, kWatchOrigin, canvas.image(), st);
        panel->draw(frame, kPanelX, *app, now);
    }
};


int runInteractive(const Options& opt) {
    sim::SimTick tick(opt.wrap ? kWrapStartTick : 0u);
    Simulator s(kOdometerStart);
    s.trace.printToStdout = opt.trace;
    s.build(std::make_unique<sim::SimWallClock>(), true);
    s.app->init(tick.now());

    cvui::init(kWindow);
    s.sensors.runThread(tick);
#ifdef _WIN32
    const HWND hwnd = FindWindowA(nullptr, kWindow);
#else
    // No async key-state API here: cv::waitKeyEx only reports presses, so a
    // press is held as a short pulse. Long press: hold the on-screen button.
    constexpr ap::TickMs kKeyPulseMs = 120;
    std::array<ap::TickMs, ap::kKeyCount> keyUntil{};
    std::array<bool, ap::kKeyCount> keyHeld{};
#endif

    for (;;) {
        const ap::TickMs now = tick.now();

        KeyLevels levels{};
#ifdef _WIN32
        if (GetForegroundWindow() == hwnd) {
            auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
            levels[ap::keyIndex(ap::KeyId::Light)] = down(VK_SPACE);
            levels[ap::keyIndex(ap::KeyId::Up)] = down(VK_UP);
            levels[ap::keyIndex(ap::KeyId::Down)] = down(VK_DOWN);
            levels[ap::keyIndex(ap::KeyId::Start)] = down(VK_RETURN);
            levels[ap::keyIndex(ap::KeyId::Back)] = down(VK_BACK);
        }
#else
        for (std::size_t i = 0; i < ap::kKeyCount; ++i) {
            if (keyHeld[i] && ap::timeReached(now, keyUntil[i])) keyHeld[i] = false;
            levels[i] = keyHeld[i];
        }
#endif
        ap::KeyId hit{};
        if (cvui::mouse(kWindow, cvui::LEFT_BUTTON, cvui::IS_DOWN) && s.watch.hit(kWatchOrigin, cvui::mouse(kWindow), hit)) {
            levels[ap::keyIndex(hit)] = true;
        }
        s.setKeys(levels, now);

        s.tick(now);
        s.compose(now);
        cvui::update(kWindow);
        cv::imshow(kWindow, s.frame);

#ifdef _WIN32
        const int key = cv::waitKey(15);
        if (key == 27) break;
#else
        const int key = cv::waitKeyEx(15);
        if (key == 27) break;
        ap::KeyId pressed{};
        bool mapped = true;
        switch (key) {
            case ' ':              pressed = ap::KeyId::Light; break;
            case 13: case 10:      pressed = ap::KeyId::Start; break;
            case 8: case 127:      pressed = ap::KeyId::Back;  break;
            case 0xF700: case 0x260000: pressed = ap::KeyId::Up;   break;  // Cocoa / GTK-style codes
            case 0xF701: case 0x280000: pressed = ap::KeyId::Down; break;
            default: mapped = false; break;
        }
        if (mapped) {
            const std::size_t i = ap::keyIndex(pressed);
            keyHeld[i] = true;
            keyUntil[i] = static_cast<ap::TickMs>(tick.now() + kKeyPulseMs);
        }
#endif
        if (cv::getWindowProperty(kWindow, cv::WND_PROP_VISIBLE) < 1.0) break;
    }
    s.sensors.stopThread();
    cv::destroyAllWindows();
    return 0;
}


int runScript(const Options& opt) {
    std::ifstream in(opt.script);
    if (!in) {
        std::cerr << "cannot open script " << opt.script << '\n';
        return 2;
    }
    std::filesystem::create_directories(opt.outDir);

    ap::TickMs now = opt.wrap ? kWrapStartTick : 0u;
    ap::TickMs elapsed = 0;
    Simulator s(kOdometerStart);
    s.trace.printToStdout = true;
    s.build(std::make_unique<sim::SimWallClock>(ap::LocalTime{2026, 11, 18, 3, 8, 42, 0}, &elapsed), false);
    s.app->init(now);
    cvui::init(kWindow, -1, false, true);

    auto advance = [&](uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 20) {
            now += 20;
            elapsed += 20;
            s.sensors.step(now);
            s.tick(now);
        }
    };
    auto press = [&](ap::KeyId k, uint32_t holdMs) {
        KeyLevels levels{};
        levels[ap::keyIndex(k)] = true;
        s.setKeys(levels, now);
        advance(holdMs);
        s.setKeys(KeyLevels{}, now);
        advance(60);
    };

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (const auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        std::istringstream ss(line);
        std::string cmd, arg;
        if (!(ss >> cmd)) continue;
        ss >> arg;
        ap::KeyId k{};
        if (cmd == "wait") {
            advance(static_cast<uint32_t>(std::stoul(arg)));
        } else if ((cmd == "press" || cmd == "hold") && parseKey(arg, k)) {
            uint32_t hold = (cmd == "hold") ? 1000u : 80u;
            if (uint32_t h = 0; ss >> h) hold = h;
            press(k, hold);
            advance(340);
        } else if (cmd == "double" && parseKey(arg, k)) {
            press(k, 60);
            press(k, 60);
            advance(340);
        } else if (cmd == "hr") {
            if (arg == "off") s.world.hrWorn = false;
            else if (arg == "on") s.world.hrWorn = true;
            else s.world.targetHr = std::stoi(arg);
        } else if (cmd == "speed") {
            s.world.speedKmh = std::stod(arg);
        } else if (cmd == "gps") {
            s.world.gpsSignal = (arg != "off");
        } else if (cmd == "fault") {
            std::string v;
            ss >> v;
            const bool on = (v != "off");
            if (arg == "gps") s.world.gpsHardwareFault = on;
            else if (arg == "storage") s.recorder->failBegin = on;
            else if (arg == "save") s.recorder->failFinish = on;
        } else if (cmd == "history" && arg == "load") {
            std::cout << "[history] loaded " << s.recorder->loadExisting() << " activities\n";
        } else if (cmd == "shot") {
            s.app->render(s.canvas);
            s.compose(now);
            const std::string path = (std::filesystem::path(opt.outDir) / arg).string();
            cv::imwrite(path, s.frame);
            std::cout << "[shot] " << path << "  screen=" << s.app->topScreenName()
                      << "  state=" << ap::toString(s.app->activityState()) << '\n';
        } else {
            std::cerr << opt.script << ':' << lineNo << ": unknown command '" << cmd << "'\n";
            return 3;
        }
    }
    return 0;
}

}

int main(int argc, char** argv) {
    const Options opt = parse(argc, argv);
    return opt.script.empty() ? runInteractive(opt) : runScript(opt);
}
