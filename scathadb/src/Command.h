#ifndef SVM_COMMAND_H_
#define SVM_COMMAND_H_

#include <functional>
#include <span>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace sdb {

class Debugger;

///
struct Command {
    static std::span<Command const> All() { return _all; }

    Command(std::string hotkey,
            std::function<std::string(Debugger const&)> buttonLabel,
            std::function<bool(Debugger const&)> isActive,
            std::function<void(Debugger&)> action);

    ///
    std::string hotkey;

    ///
    std::function<std::string(Debugger const&)> buttonLabel;

    ///
    std::function<bool(Debugger const&)> isActive;

    ///
    std::function<void(Debugger&)> action;

private:
    static std::vector<Command> _all;
};

///
ftxui::Component ToolbarButton(Debugger* debugger, Command command);

} // namespace sdb

#endif // SVM_COMMAND_H_
