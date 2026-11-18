#include "ap/input/input_router.h"

#include <cstdint>

namespace ap {

bool InputRouter::append(IInputHandler& h) {
    if (m_count == m_chain.size()) return false;
    for (std::size_t i = 0; i < m_count; ++i) {
        if (m_chain[i] == &h) return false;
    }
    m_chain[m_count++] = &h;
    return true;
}

IInputHandler* InputRouter::route(const GestureEvent& e) {
    if (!isValid(e.key)) return nullptr;
    for (std::size_t i = 0; i < m_count; ++i) {
        if (m_chain[i]->handle(e)) return m_chain[i];
    }
    if (m_unhandled != UINT16_MAX) ++m_unhandled;
    return nullptr;
}

}  // namespace ap
