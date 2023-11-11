#include "Command.h"

#include <utl/utility.hpp>

#include "Common.h"

using namespace sdb;
using namespace ftxui;

Command::Command(std::string hotkey,
                 std::function<std::string(Debugger const&)> buttonLabel,
                 std::function<bool(Debugger const&)> isActive,
                 std::function<void(Debugger&)> action):
    hotkey(std::move(hotkey)),
    buttonLabel(std::move(buttonLabel)),
    isActive(std::move(isActive)),
    action(std::move(action)) {
    _all.push_back(*this);
}

std::vector<Command> Command::_all;

Component sdb::ToolbarButton(Debugger* debugger, Command command) {
    auto opt = ButtonOption::Simple();
    opt.transform = [=](EntryState const&) {
        auto str = command.buttonLabel(*debugger);
        auto elem = text(str) | bold;
        if (!command.isActive(*debugger)) {
            elem |= color(Color::GrayDark);
        }
        return elem | center |
               size(WIDTH, EQUAL, utl::narrow_cast<int>(str.size() + 2));
    };
    auto callback = [=] {
        if (command.isActive(*debugger)) {
            command.action(*debugger);
        }
        else {
            beep();
        }
    };
    return Button("Button", callback, opt);
}
