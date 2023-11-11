#include "Debugger.h"

#include "Command.h"
#include "HelpPanel.h"
#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

// clang-format off
auto const QuitCmd = Command::Add({
    "q",
    [](Debugger const& db) { return " X "; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.quit(); },
    "Quit the debugger"
});

auto const RunCmd = Command::Add({
    "r",
    [](Debugger const& db) { return "Run"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.model()->run(); },
    "Run the currently loaded program"
});

auto const StopCmd = Command::Add({
    "x",
    [](Debugger const& db) { return "Stop"; },
    [](Debugger const& db) { return db.model()->isActive(); },
    [](Debugger& db) { db.model()->shutdown(); },
    "Stop the currently running program"
});

auto const OpenCmd = Command::Add({
    "o",
    [](Debugger const& db) { return "Open"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.openModal("file-open"); },
    "Open an executable file for debugging"
});

auto const SettingsCmd = Command::Add({
    ",",
    [](Debugger const& db) { return "Settings"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.openModal("settings"); },
    "Show the settings window"
});

auto const HelpCmd = Command::Add({
    "h",
    [](Debugger const& db) { return "Help"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.openModal("help"); },
    "Show this help panel"
});

auto const ToggleExecCmd = Command::Add({
    "p",
    [](Debugger const& db) { return db.model()->isSleeping() ? "|>" : "||"; },
    [](Debugger const& db) { return db.model()->isActive(); },
    [](Debugger& db) { db.model()->toggleExecution(); },
    "Toggle execution"
});

auto const StepCmd = Command::Add({
    "s",
    [](Debugger const& db) { return ">_"; },
    [](Debugger const& db) {
        return db.model()->isActive() && db.model()->isSleeping();
    },
    [](Debugger& db) { db.model()->skipLine(); },
    "Execute one execution step (one instruction)"
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
    addModal("help", HelpPanel());
    auto sidebar = Container::Vertical(
        { FlagsView(model()), sdb::separator(), RegisterView(model()) });
    auto centralSplit = splitRight(sidebar, instView, 30);
    auto toolbar = Toolbar({
        ToolbarButton(this, QuitCmd),
        ToolbarButton(this, RunCmd),
        ToolbarButton(this, StopCmd),

        sdb::spacer(),

        Renderer([=] { return text(model()->currentFilepath().string()); }),

        sdb::spacer(),

        ToolbarButton(this, OpenCmd),
        ToolbarButton(this, SettingsCmd),
        ToolbarButton(this, HelpCmd),
    });
    auto top = Container::Vertical({
        sdb::separator(),
        toolbar,
        sdb::separator(),
        centralSplit | flex,
    });
    auto dbgCtrlBar = Toolbar({
        ToolbarButton(this, ToggleExecCmd),
        ToolbarButton(this, StepCmd),
    });
    auto bottom = Container::Vertical(
        { dbgCtrlBar, sdb::separator(), ConsoleView(model()) });
    root = splitBottom(bottom, top);
    root |= Command::EventCatcher(this);
    for (auto& [name, panel]: modalViews) {
        root |= panel.overlay();
    }
    /// Instruction view is focused by default
    instView->TakeFocus();
}

void Debugger::run() { screen.Loop(root); }

void Debugger::quit() {
    model()->shutdown();
    screen.Exit();
}
