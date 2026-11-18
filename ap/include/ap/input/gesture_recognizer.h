#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "ap/core/ring_buffer.h"
#include "ap/input/input_types.h"

namespace ap {

struct GestureConfig {
    uint16_t longPressMs = 800;
    uint16_t doubleWindowMs = 300;
};

class GestureRecognizer {
public:
    static constexpr std::size_t kQueueCapacity = 8;

    explicit GestureRecognizer(const GestureConfig& cfg = GestureConfig{});

    void setDoubleEnabled(KeyId key, bool enabled);

    bool onKeyEvent(const KeyEvent& e);

    void poll(TickMs nowMs);

    std::optional<GestureEvent> next() { return m_out.pop(); }
    std::size_t pending() const { return m_out.size(); }
    uint16_t dropped() const { return m_dropped; }

    void reset();

private:
    enum class KeyState : uint8_t {
        Idle,
        Pressed,
        LongFired,
        WaitSecond,
        SecondDown,
    };
    struct KeyCtx {
        KeyState state = KeyState::Idle;
        TickMs since = 0;
        TickMs pressedAt = 0;
        bool doubleEnabled = false;
    };

    void emit(KeyId key, Gesture g, TickMs t);

    GestureConfig m_cfg;
    std::array<KeyCtx, kKeyCount> m_keys{};
    RingBuffer<GestureEvent, kQueueCapacity> m_out;
    uint16_t m_dropped = 0;
};

}
