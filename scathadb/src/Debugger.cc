#include "Debugger.h"

#include "Command.h"
#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

// clang-format off
auto const QuitCmd = Command::Add({
    "q",
    [](Debugger const& db) { return " X "; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.quit(); }
});

auto const RunCmd = Command::Add({
    "r",
    [](Debugger const& db) { return "Run"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.model()->run(); }
});

auto const StopCmd = Command::Add({
    "x",
    [](Debugger const& db) { return "Stop"; },
    [](Debugger const& db) { return db.model()->isActive(); },
    [](Debugger& db) { db.model()->shutdown(); }
});

auto const OpenCmd = Command::Add({
    "o",
    [](Debugger const& db) { return "Open"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.openModal("file-open"); }
});

auto const SettingsCmd = Command::Add({
    ",",
    [](Debugger const& db) { return "Settings"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.openModal("settings"); }
});

auto const ToggleExecCmd = Command::Add({
    "p",
    [](Debugger const& db) { return db.model()->isSleeping() ? "|>" : "||"; },
    [](Debugger const& db) { return db.model()->isActive(); },
    [](Debugger& db) { db.model()->toggleExecution(); },
});

auto const StepCmd = Command::Add({
    "s",
    [](Debugger const& db) { return ">_"; },
    [](Debugger const& db) {
        return db.model()->isActive() && db.model()->isSleeping();
    },
    [](Debugger& db) { db.model()->skipLine(); }
});
// clang-format on

Debugger::Debugger(Model* _model):
    screen(ScreenInteractive::Fullscreen()),
    _model(_model),
    instView(InstructionView(model())) {
    model()->setRefreshCallback(
        [this] { screen.PostEvent(Event::Special("Wakeup call")); });
    model()->setReloadCallback([this] { instView->refresh(); });
    addModal("file-open", OpenFilePanel(model()));
    addModal("settings", SettingsView());
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
    root = splitBottom(bottom, top);
    root |= Command::EventCatcher(this);
    for (auto& [name, panel]: modalViews) {
        root |= panel.overlay();
    }
}

void Debugger::run() { screen.Loop(root); }

void Debugger::quit() {
    model()->shutdown();
    screen.Exit();
}
