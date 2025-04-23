#include "App/Command.h"

#include <utl/utility.hpp>

#include "UI/Common.h"
#include "Views/HelpPanel.h"

using namespace sdb;

Command const& Command::Add(Command cmd) {
    addPanelCommandsInfo("Global commands",
                         { .hotkey = cmd.hotkey, .message = cmd.description });
    _all.push_back(std::move(cmd));
    return _all.back();
}

ftxui::ComponentDecorator Command::EventCatcher(Debugger* db) {
    return ftxui::CatchEvent([=](ftxui::Event event) {
        if (event.is_character()) {
            for (auto& cmd: Command::All()) {
                if (event.character() != cmd.hotkey) {
                    continue;
                }
                if (cmd.isActive(*db)) {
                    cmd.action(*db);
                }
                else {
                    beep();
                }
                return true;
            }
            return false;
        }
        return false;
    });
}

std::vector<Command> Command::_all;

ftxui::Component sdb::ToolbarButton(Debugger* debugger, Command command) {
    auto opt = ftxui::ButtonOption::Simple();
    opt.transform = [=](ftxui::EntryState const&) {
        auto str = command.buttonLabel(*debugger);
        auto elem = ftxui::text(str) | ftxui::bold;
        if (!command.isActive(*debugger)) {
            elem |= ftxui::dim;
        }
        return elem | ftxui::center |
               ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                           utl::narrow_cast<int>(str.size() + 2));
    };
    auto callback = [=] {
        if (command.isActive(*debugger)) {
            command.action(*debugger);
        }
        else {
            beep();
        }
    };
    return ftxui::Button("Button", callback, opt);
}
