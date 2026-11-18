#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/input/input_types.h"

namespace ap {

// ---- Chain of Responsibility node ---------------------------------------------
class IInputHandler {
public:
    // true = consumed: handlers after this one never see the event.
    virtual bool handle(const GestureEvent& e) = 0;
    virtual const char* handlerName() const = 0;

    IInputHandler(const IInputHandler&) = delete;
    IInputHandler& operator=(const IInputHandler&) = delete;

protected:
    IInputHandler() = default;
    ~IInputHandler() = default;
};

// Routes each gesture through an ordered, fixed chain:
//
//   KeyLock  ->  Overlay (alerts/toasts)  ->  Navigator (top screen)  ->  Global (backlight)
//
// "What does BACK mean right now?" is answered by *position in the chain*,
// not by an if/else ladder that knows every screen.
//
// The chain is assembled once at init. Handlers that change behaviour at
// runtime (overlay shown/hidden, screen pushed) do it internally, so the
// chain itself is never mutated while route() iterates it.
class InputRouter {
public:
    static constexpr std::size_t kMaxHandlers = 6;

    bool append(IInputHandler& h);  // false when full or already present

    // Returns the handler that consumed the event, or nullptr.
    IInputHandler* route(const GestureEvent& e);

    std::size_t size() const { return m_count; }
    uint16_t unhandled() const { return m_unhandled; }

private:
    std::array<IInputHandler*, kMaxHandlers> m_chain{};
    std::size_t m_count = 0;
    uint16_t m_unhandled = 0;
};

// ---- Command: an action as an object --------------------------------------------
// Menus hold ICommand* per item; the menu does not know what "Save" means.
// Commands borrow their receiver (e.g. IActivityControl) - they own nothing.
class ICommand {
public:
    virtual void execute(TickMs nowMs) = 0;

    ICommand(const ICommand&) = delete;
    ICommand& operator=(const ICommand&) = delete;

protected:
    ICommand() = default;
    ~ICommand() = default;
};

}  // namespace ap
