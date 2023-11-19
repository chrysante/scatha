#include "App/Debugger.h"

#include <cassert>

#include "App/Command.h"
#include "Model/Model.h"
#include "Views/HelpPanel.h"
#include "Views/Views.h"

using namespace sdb;
using namespace ftxui;

// clang-format off
auto const QuitCmd = Command::Add({
    "q",
    [](Debugger const& db) { return "Quit"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.openModal("quit-confirm"); },
    "Quit the debugger"
});

auto const RunCmd = Command::Add({
    "r",
    [](Debugger const& db) { return "Run"; },
    [](Debugger const& db) { return !db.model()->disassembly().empty(); },
    [](Debugger& db) { db.model()->start(); },
    "Run the currently loaded program"
});

auto const StopCmd = Command::Add({
    "x",
    [](Debugger const& db) { return "Stop"; },
    [](Debugger const& db) { return !db.model()->isStopped(); },
    [](Debugger& db) { db.model()->stop(); },
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

auto const ToggleLeftSidebarCmd = Command::Add({
    "L",
    [](Debugger const& db) { return "⌷⎕"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.toggleSidebar(0); },
    "Show or hide the left sidebar"
});

auto const ToggleRightSidebarCmd = Command::Add({
    "R",
    [](Debugger const& db) { return "⎕⌷"; },
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.toggleSidebar(1); },
    "Show or hide the right sidebar"
});

auto const ToggleConsoleCmd = Command::Add({
    "C",
    [](Debugger const& db) { return "▂▂"; }, 
    [](Debugger const& db) { return true; },
    [](Debugger& db) { db.toggleBottombar(); },
    "Show or hide the console"
});

auto const ToggleExecCmd = Command::Add({
    "p",
    [](Debugger const& db) { return db.model()->isPaused() ? "|>" : "||"; },
    [](Debugger const& db) { return !db.model()->isStopped(); },
    [](Debugger& db) { db.model()->toggle(); },
    "Toggle execution"
});

auto const StepCmd = Command::Add({
    "s",
    [](Debugger const& db) { return ">_"; },
    [](Debugger const& db) {
        return db.model()->isPaused();
    },
    [](Debugger& db) { db.model()->stepInstruction(); },
    "Execute one execution step (one instruction)"
});
// clang-format on

Debugger::Debugger(Model* _model):
    _screen(ScreenInteractive::Fullscreen()), _model(_model) {
    uiHandle.addRefreshCallback(
        [this] { _screen.PostEvent(Event::Special("Refresh")); });
    _model->setUIHandle(&uiHandle);
    addModal("file-open", OpenFilePanel(model()));
    addModal("settings", SettingsView());
    addModal("help", HelpPanel());
    addModal("quit-confirm", QuitConfirm([=] { quit(); }));
    auto rightSidebar = TabView({ { " VM State ", VMStateView(model()) },
                                  { " Callstack ", Renderer([] {
                                        return placeholder("Not Implemented");
                                    }) } });
    auto instView = InstructionView(model(), uiHandle);
    auto centralSplit = SplitRight(rightSidebar, instView, &_sidebarSize[1]);
    auto toolbar = Toolbar({
        ToolbarButton(this, QuitCmd),
        ToolbarButton(this, RunCmd),
        ToolbarButton(this, StopCmd),
        sdb::Spacer(),
        Renderer(
            [=] { return text(model()->currentFilepath().string()) | flex; }),
        sdb::Spacer(),
        ToolbarButton(this, OpenCmd),
        ToolbarButton(this, SettingsCmd),
        ToolbarButton(this, HelpCmd),
        ToolbarButton(this, ToggleRightSidebarCmd),
    });
    auto top = Container::Vertical({
        sdb::Separator(),
        toolbar,
        sdb::Separator(),
        centralSplit | flex,
    });
    auto dbgCtrlBar = Toolbar({
        ToolbarButton(this, ToggleExecCmd),
        ToolbarButton(this, StepCmd),
        Spacer(),
        ToolbarButton(this, ToggleConsoleCmd),
    });
    auto bottom = Container::Vertical(
        { dbgCtrlBar, sdb::Separator(), ConsoleView(model()) });
    root = SplitBottom(bottom, top, &_bottombarSize);
    root |= Command::EventCatcher(this);
    for (auto& [name, panel]: modalViews) {
        root |= panel.overlay();
    }
    /// Instruction view is focused by default
    instView->TakeFocus();
}

void Debugger::run() { _screen.Loop(root); }

void Debugger::quit() {
    model()->stop();
    _screen.Exit();
}

void Debugger::toggleSidebar(size_t index) {
    assert(index < 2);
    int const min = -1;
    if (_sidebarSizeBackup[index] <= min) {
        _sidebarSizeBackup[index] = 30;
    }
    if (_sidebarSize[index] <= min) {
        _sidebarSize[index] = _sidebarSizeBackup[index];
    }
    else {
        _sidebarSizeBackup[index] = _sidebarSize[index];
        _sidebarSize[index] = min;
    }
}

void Debugger::toggleBottombar() {
    int const min = 2;
    if (_bottombarSizeBackup <= min) {
        _bottombarSizeBackup = 10;
    }
    if (_bottombarSize <= min) {
        _bottombarSize = _bottombarSizeBackup;
    }
    else {
        _bottombarSizeBackup = _bottombarSize;
        _bottombarSize = min;
    }
}
