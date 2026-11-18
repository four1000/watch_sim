#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ap/input/input_types.h"

namespace ap {

class IInputHandler {
public:
    virtual bool handle(const GestureEvent& e) = 0;
    virtual const char* handlerName() const = 0;

    IInputHandler(const IInputHandler&) = delete;
    IInputHandler& operator=(const IInputHandler&) = delete;

protected:
    IInputHandler() = default;
    ~IInputHandler() = default;
};

class InputRouter {
public:
    static constexpr std::size_t kMaxHandlers = 6;

    bool append(IInputHandler& h);

    IInputHandler* route(const GestureEvent& e);

    std::size_t size() const { return m_count; }
    uint16_t unhandled() const { return m_unhandled; }

private:
    std::array<IInputHandler*, kMaxHandlers> m_chain{};
    std::size_t m_count = 0;
    uint16_t m_unhandled = 0;
};

class ICommand {
public:
    virtual void execute(TickMs nowMs) = 0;

    ICommand(const ICommand&) = delete;
    ICommand& operator=(const ICommand&) = delete;

protected:
    ICommand() = default;
    ~ICommand() = default;
};
}
