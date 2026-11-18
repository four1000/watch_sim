#pragma once

#include "ap/activity/activity_session.h"
#include "ap/input/input_router.h"
#include "ap/ui/navigator.h"

namespace ap {

class ActivityCommand final : public ICommand {
public:
    using Request = bool (IActivityControl::*)(TickMs);

    ActivityCommand(IActivityControl& control, Request request) : m_control(control), m_request(request) {}
    void execute(TickMs now) override { (m_control.*m_request)(now); }

private:
    IActivityControl& m_control;
    Request m_request;
};

class PushScreenCommand final : public ICommand {
public:
    PushScreenCommand(Navigator& nav, Screen& target) : m_nav(nav), m_target(target) {}
    void execute(TickMs now) override { m_nav.push(m_target, now); }

private:
    Navigator& m_nav;
    Screen& m_target;
};
}
