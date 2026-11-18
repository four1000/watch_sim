#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "ap/core/ring_buffer.h"
#include "ap/input/input_types.h"

namespace ap {

struct GestureConfig {
    uint16_t longPressMs = 800;     // held this long -> Long (fires while still held)
    uint16_t doubleWindowMs = 300;  // release -> next press within this window -> Double
};

// KeyEvent (Down/Up edges + time) -> GestureEvent (Short / Long / Double).
//
// One small state machine per key, so two keys held together do not interfere.
// Double detection is opt-in per key because it costs latency: a Short on a
// double-enabled key can only be emitted after the window expires.
class GestureRecognizer {
public:
    static constexpr std::size_t kQueueCapacity = 8;

    explicit GestureRecognizer(const GestureConfig& cfg = GestureConfig{});

    void setDoubleEnabled(KeyId key, bool enabled);

    // Invalid key/edge -> false. Edges that make no sense in the current state
    // (Up while idle, Down while already down) are ignored -> true.
    bool onKeyEvent(const KeyEvent& e);

    // Must also run without edges: detects "still held" long presses and
    // expired double-click windows.
    void poll(TickMs nowMs);

    std::optional<GestureEvent> next() { return m_out.pop(); }
    std::size_t pending() const { return m_out.size(); }
    uint16_t dropped() const { return m_dropped; }  // saturates at UINT16_MAX

    // Recover from a lost edge (display off, driver queue overflow).
    void reset();

private:
    enum class KeyState : uint8_t {
        Idle,
        Pressed,       // down, long threshold not reached yet
        LongFired,     // Long already emitted, waiting for release
        WaitSecond,    // released after a short press, double window open
        SecondDown,    // second press inside the window
    };
    struct KeyCtx {
        KeyState state = KeyState::Idle;
        TickMs since = 0;      // start of the current state (press or release time)
        TickMs pressedAt = 0;  // first press of the gesture = the gesture's timestamp
        bool doubleEnabled = false;
    };

    void emit(KeyId key, Gesture g, TickMs t);

    GestureConfig m_cfg;
    std::array<KeyCtx, kKeyCount> m_keys{};
    RingBuffer<GestureEvent, kQueueCapacity> m_out;
    uint16_t m_dropped = 0;
};

}  // namespace ap
