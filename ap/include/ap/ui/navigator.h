#pragma once

#include <array>
#include <cstddef>

#include "ap/input/input_router.h"
#include "ap/ui/screen.h"

namespace ap {

class Navigator final : public IInputHandler {
public:
    static constexpr std::size_t kMaxDepth = 6;

    bool push(Screen& s, TickMs now);
    bool pop(TickMs now);
    void resetTo(Screen& root, TickMs now);
    bool popUntil(const Screen& s, TickMs now);

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
}
