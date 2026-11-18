#pragma once

#include <cstddef>
#include <cstdint>

#include "ap/core/time.h"

namespace ap {

enum class KeyId : uint8_t { Light, Up, Down, Start, Back, Count };
constexpr std::size_t kKeyCount = static_cast<std::size_t>(KeyId::Count);

constexpr std::size_t keyIndex(KeyId k) { return static_cast<std::size_t>(k); }
constexpr bool isValid(KeyId k) { return keyIndex(k) < kKeyCount; }

enum class KeyEdge : uint8_t { Down, Up };

struct KeyEvent {
    KeyId key;
    KeyEdge edge;
    TickMs timestampMs;
};

enum class Gesture : uint8_t { Short, Long, Double };

struct GestureEvent {
    KeyId key;
    Gesture gesture;
    TickMs timestampMs;
};

const char* toString(KeyId k);
const char* toString(Gesture g);
}
