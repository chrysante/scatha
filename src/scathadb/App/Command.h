#ifndef SCATHADB_APP_COMMAND_H_
#define SCATHADB_APP_COMMAND_H_

#include <functional>
#include <span>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace sdb {

class Debugger;

///
struct Command {
    ///
    static std::span<Command const> All() { return _all; }

    ///
    static Command const& Add(Command cmd);

    ///
    static ftxui::ComponentDecorator EventCatcher(Debugger* db);

    ///
    std::string hotkey;

    ///
    std::function<std::string(Debugger const&)> buttonLabel;

    ///
    std::function<bool(Debugger const&)> isActive;

    ///
    std::function<void(Debugger&)> action;

    ///
    std::string description;

private:
    static std::vector<Command> _all;
};

///
ftxui::Component ToolbarButton(Debugger* debugger, Command command);

} // namespace sdb

#endif // SCATHADB_APP_COMMAND_H_
