#include "Debugger.h"

#include "Command.h"
#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

// clang-format off
auto const QuitCmd = Command{
    "q",
    [](Debugger const& db) { return " X "; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) {
        db.model()->shutdown();
        db.screen.Exit();
    }
};

auto const RunCmd = Command{
    "r",
    [](Debugger const& db) { return "Run"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.model()->run(); }
};

auto const StopCmd = Command{
    "x",
    [](Debugger const& db) { return "Stop"; },
    [](Debugger const& db) { return db.model()->isActive(); },
    [](Debugger& db) { db.model()->shutdown(); }
};

auto const OpenCmd = Command{
    "o",
    [](Debugger const& db) { return "Open"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.fileOpenPanel.open(); }
};

auto const SettingsCmd = Command{
    ",",
    [](Debugger const& db) { return "Settings"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.settingsView.open(); }
};

auto const ToggleExecCmd = Command{
    "p",
    [](Debugger const& db) { return db.model()->isSleeping() ? "|>" : "||"; },
    [](Debugger const& db) { return db.model()->isActive(); },
    [](Debugger& db) { db.model()->toggleExecution(); },
};

auto const StepCmd = Command{
    "s",
    [](Debugger const& db) { return ">_"; },
    [](Debugger const& db) {
        return db.model()->isActive() && db.model()->isSleeping();
    },
    [](Debugger& db) { db.model()->skipLine(); }
};
// clang-format on

Debugger::Debugger(Model* _model):
    _model(_model),
    fileOpenPanel(OpenFilePanel(model())),
    settingsView(SettingsView()),
    screen(ScreenInteractive::Fullscreen()),
    instView(InstructionView(model())) {
    model()->setRefreshCallback(
        [this] { screen.PostEvent(Event::Special("Wakeup call")); });
    model()->setReloadCallback([this] { instView->refresh(); });
    auto sidebar = Container::Vertical(
        { FlagsView(model()), sdb::separator(), RegisterView(model()) });
    auto centralSplit = splitRight(sidebar, instView, 30);
    auto toolbar = Toolbar({
        ToolbarButton(this, QuitCmd),
        sdb::separator(),
        ToolbarButton(this, RunCmd),
        sdb::separatorEmpty(),
        ToolbarButton(this, StopCmd),
        sdb::spacer(),
        ToolbarButton(this, OpenCmd),
        sdb::separatorEmpty(),
        ToolbarButton(this, SettingsCmd),
    });

    auto top = Container::Vertical({
        sdb::separator(),
        toolbar,
        sdb::separator(),
        centralSplit | flex,
    });
    auto dbgCtrlBar = Toolbar({
        ToolbarButton(this, ToggleExecCmd),
        sdb::separatorEmpty(),
        ToolbarButton(this, StepCmd),
    });
    auto bottom = Container::Vertical(
        { dbgCtrlBar, sdb::separator(), ConsoleView(model()) });
    root = splitBottom(bottom, top) | CatchEvent([=](Event event) {
               if (event.is_character()) {
                   for (auto& [key, command]: keyCommands) {
                       if (event.character() == key) {
                           command();
                           return true;
                       }
                   }
               }
               return false;
           }) |
           fileOpenPanel.overlay() | settingsView.overlay();

    for (auto& cmd: Command::All()) {
        addKeyCommand(cmd.hotkey, [=] {
            if (cmd.isActive(*this)) {
                cmd.action(*this);
            }
            else {
                beep();
            }
        });
    }
}

void Debugger::run() { screen.Loop(root); }

void Debugger::addKeyCommand(std::string key, std::function<void()> command) {
    keyCommands.push_back({ std::move(key), std::move(command) });
}
