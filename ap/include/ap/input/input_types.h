#pragma once

#include <cstddef>
#include <cstdint>

#include "ap/core/time.h"

namespace ap {

// The five physical buttons of a typical running watch:
//   LIGHT (top-left)   START (top-right)
//   UP    (mid-left)
//   DOWN  (bottom-left) BACK (bottom-right)
enum class KeyId : uint8_t { Light, Up, Down, Start, Back, Count };
constexpr std::size_t kKeyCount = static_cast<std::size_t>(KeyId::Count);

constexpr std::size_t keyIndex(KeyId k) { return static_cast<std::size_t>(k); }
constexpr bool isValid(KeyId k) { return keyIndex(k) < kKeyCount; }

enum class KeyEdge : uint8_t { Down, Up };

// Raw edge from the key driver / simulator.
struct KeyEvent {
    KeyId key;
    KeyEdge edge;
    TickMs timestampMs;
};

enum class Gesture : uint8_t { Short, Long, Double };

// Semantic input the rest of the AP layer reacts to.
// timestampMs = when the gesture *happened* (e.g. the press time of a long
// press), not when it was processed - lap times are taken from it.
struct GestureEvent {
    KeyId key;
    Gesture gesture;
    TickMs timestampMs;
};

const char* toString(KeyId k);
const char* toString(Gesture g);

}  // namespace ap
