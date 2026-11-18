#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <random>
#include <thread>

#include "ap/services/services.h"
#include "sim/sim_services.h"

namespace sim {

struct SensorWorld {
    std::atomic<int> targetHr{75};
    std::atomic<bool> hrWorn{true};
    std::atomic<double> speedKmh{0.0};
    std::atomic<bool> gpsSignal{true};
    std::atomic<bool> gpsHardwareFault{false};
};

class SensorSimulator;

class SimGps final : public ap::IGpsService {
public:
    explicit SimGps(SensorSimulator& s) : m_sim(s) {}
    bool start() override;
    void stop() override;

private:
    SensorSimulator& m_sim;
};

class SimHeartRate final : public ap::IHeartRateService {
public:
    explicit SimHeartRate(SensorSimulator& s) : m_sim(s) {}
    bool start() override;
    void stop() override;

private:
    SensorSimulator& m_sim;
};

class SensorSimulator {
public:
    static constexpr uint32_t kPeriodMs = 1000;
    static constexpr uint32_t kGpsAcquireMs = 2500;

    SensorSimulator(SensorWorld& world, uint32_t odometerStartDm, uint32_t seed = 42);
    ~SensorSimulator();
    SensorSimulator(const SensorSimulator&) = delete;
    SensorSimulator& operator=(const SensorSimulator&) = delete;

    void connect(ap::ISensorSink& sink) { m_sink = &sink; }

    void runThread(const SimTick& tick);
    void stopThread();
    void step(ap::TickMs now);

    SimGps& gps() { return m_gps; }
    SimHeartRate& hr() { return m_hr; }

private:
    friend class SimGps;
    friend class SimHeartRate;

    void emit(ap::TickMs now);

    SensorWorld& m_world;
    ap::ISensorSink* m_sink = nullptr;
    SimGps m_gps{*this};
    SimHeartRate m_hr{*this};

    std::mutex m_mutex;
    bool m_gpsOn = false;
    bool m_hrOn = false;
    ap::TickMs m_gpsStartedAt = 0;
    bool m_gpsStartPending = false;
    bool m_hasLast = false;
    ap::TickMs m_last = 0;
    double m_hrValue = 72.0;
    double m_odometerDm;
    std::mt19937 m_rng;

    std::thread m_thread;
    std::condition_variable m_cv;
    bool m_quit = false;
};

}
