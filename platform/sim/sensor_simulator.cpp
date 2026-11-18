#include "sim/sensor_simulator.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace sim {

bool SimGps::start() {
    if (m_sim.m_world.gpsHardwareFault) return false;
    std::lock_guard<std::mutex> lock(m_sim.m_mutex);
    if (!m_sim.m_gpsOn) {
        m_sim.m_gpsOn = true;
        m_sim.m_gpsStartPending = true;  // acquisition timer starts at the next step()
    }
    return true;
}

void SimGps::stop() {
    std::lock_guard<std::mutex> lock(m_sim.m_mutex);
    m_sim.m_gpsOn = false;
}

bool SimHeartRate::start() {
    std::lock_guard<std::mutex> lock(m_sim.m_mutex);
    m_sim.m_hrOn = true;
    return true;
}

void SimHeartRate::stop() {
    std::lock_guard<std::mutex> lock(m_sim.m_mutex);
    m_sim.m_hrOn = false;
}

SensorSimulator::SensorSimulator(SensorWorld& world, uint32_t odometerStartDm, uint32_t seed)
    : m_world(world), m_odometerDm(odometerStartDm), m_rng(seed) {}

SensorSimulator::~SensorSimulator() { stopThread(); }

void SensorSimulator::runThread(const SimTick& tick) {
    m_thread = std::thread([this, &tick] {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_quit) {
            lock.unlock();
            step(tick.now());
            lock.lock();
            m_cv.wait_for(lock, std::chrono::milliseconds(100), [this] { return m_quit; });
        }
    });
}

void SensorSimulator::stopThread() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void SensorSimulator::step(ap::TickMs now) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_gpsStartPending) {
            m_gpsStartPending = false;
            m_gpsStartedAt = now;
        }
        if (!m_hasLast) {
            m_hasLast = true;
            m_last = now;
            return;
        }
        if (ap::elapsedMs(now, m_last) < kPeriodMs) return;
    }
    emit(now);
}

void SensorSimulator::emit(ap::TickMs now) {
    ap::HrSample hr{};
    ap::GpsSample gps{};
    bool sendHr = false;
    bool sendGps = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const double dt = ap::elapsedMs(now, m_last) / 1000.0;
        m_last = now;
        std::normal_distribution<double> noise(0.0, 1.0);

        // HR follows the slider with a first-order lag (~5 s) plus noise.
        const double target = static_cast<double>(m_world.targetHr.load());
        m_hrValue += (target - m_hrValue) * std::min(1.0, dt / 5.0) + noise(m_rng) * 0.8;
        if (m_hrOn) {
            sendHr = true;
            hr.timestampMs = now;
            hr.valid = m_world.hrWorn.load();
            hr.bpm = hr.valid ? static_cast<uint8_t>(std::clamp(std::lround(m_hrValue), 30L, 220L)) : 0;
        }

        // The odometer keeps counting while the GPS is on, like a real service,
        // and wraps modulo 2^32 (fmod keeps the double in range).
        if (m_gpsOn) {
            sendGps = true;
            const double speedDmS = m_world.speedKmh.load() * 10000.0 / 3600.0;
            const double moved = std::max(0.0, speedDmS * dt * (1.0 + noise(m_rng) * 0.03));
            m_odometerDm = std::fmod(m_odometerDm + moved, 4294967296.0);
            const bool acquired = ap::elapsedMs(now, m_gpsStartedAt) >= kGpsAcquireMs;
            gps.timestampMs = now;
            gps.hasFix = acquired && m_world.gpsSignal.load();
            gps.odometerDm = static_cast<uint32_t>(m_odometerDm);
            gps.accuracyM = static_cast<uint16_t>(gps.hasFix ? 3 + (m_rng() % 6) : 999);
        }
    }
    // Post outside our lock: the sink has its own (short) critical section.
    if (m_sink == nullptr) return;
    if (sendHr) m_sink->postHr(hr);
    if (sendGps) m_sink->postGps(gps);
}

}  // namespace sim
