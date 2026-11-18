#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace ap {

template <typename T, std::size_t N>
class RingBuffer {
    static_assert(N > 0, "capacity must be > 0");
    static_assert(std::is_trivially_copyable_v<T>,
                  "RingBuffer stores PODs only: copying must never allocate or throw");

public:
    static constexpr std::size_t kCapacity = N;

    bool push(const T& v) {
        if (m_size == N) return false;
        m_items[m_tail] = v;
        m_tail = (m_tail + 1) % N;
        ++m_size;
        return true;
    }

    std::optional<T> pop() {
        if (m_size == 0) return std::nullopt;
        const T v = m_items[m_head];
        m_head = (m_head + 1) % N;
        --m_size;
        return v;
    }

    void clear() { m_head = m_tail = m_size = 0; }

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }
    bool full() const { return m_size == N; }

private:
    std::array<T, N> m_items{};
    std::size_t m_head = 0;
    std::size_t m_tail = 0;
    std::size_t m_size = 0;
};
}
