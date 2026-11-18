#pragma once

namespace ap::osal {

// Minimal OS abstraction: the AP layer never includes <mutex> or an RTOS header.
// Simulator -> std::mutex, watch -> RTOS mutex, tests -> NullMutex.
class IMutex {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;

    IMutex(const IMutex&) = delete;
    IMutex& operator=(const IMutex&) = delete;

protected:
    IMutex() = default;
    ~IMutex() = default;
};

// RAII: unlock on every return path.
class LockGuard {
public:
    explicit LockGuard(IMutex& m) : m_mutex(m) { m_mutex.lock(); }
    ~LockGuard() { m_mutex.unlock(); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    IMutex& m_mutex;
};

}  // namespace ap::osal
