#pragma once

#include <array>
#include <cstddef>

#include "ap/input/input_router.h"
#include "ap/ui/screen.h"

namespace ap {

// Screen stack. One link of the input chain: forwards gestures to the top
// screen; an unconsumed BACK pops (so screens get "back" for free).
//
// push/pop call exit()/enter() so a screen's lifecycle hooks always run in
// pairs. Screens are owned elsewhere (WatchApp members) - the stack only
// borrows them, so popping never destroys anything.
class Navigator final : public IInputHandler {
public:
    static constexpr std::size_t kMaxDepth = 6;

    bool push(Screen& s, TickMs now);      // false: full, or already on the stack
    bool pop(TickMs now);                  // false: would leave the stack empty
    void resetTo(Screen& root, TickMs now);
    bool popUntil(const Screen& s, TickMs now);  // pop until `s` is on top; false if absent

    Screen* top() const { return m_depth ? m_stack[m_depth - 1] : nullptr; }
    std::size_t depth() const { return m_depth; }
    bool contains(const Screen& s) const;

    void render(ICanvas& c, const UiModel& m) const;

    bool handle(const GestureEvent& e) override;
    const char* handlerName() const override { return "Navigator"; }

private:
    std::array<Screen*, kMaxDepth> m_stack{};
    std::size_t m_depth = 0;
};

}  // namespace ap
