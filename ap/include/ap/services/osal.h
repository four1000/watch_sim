#pragma once

namespace ap::osal {

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

class LockGuard {
public:
    explicit LockGuard(IMutex& m) : m_mutex(m) { m_mutex.lock(); }
    ~LockGuard() { m_mutex.unlock(); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    IMutex& m_mutex;
};
}
