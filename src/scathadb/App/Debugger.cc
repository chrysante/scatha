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
    .hotkey = "q",
    .buttonLabel = [](Debugger const&) { return "Quit"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.openModal("quit-confirm"); },
    .description = "Quit the debugger"
});

auto const UnloadProgramCmd = Command::Add({
    .hotkey = "u",
    .buttonLabel = [](Debugger const&) { return "Unload"; },
    .isActive = [](Debugger const& db) {
        return db.model()->isProgramLoaded();
    },
    .action = [](Debugger& db) { db.openModal("unload-confirm"); },
    .description = "Unload the current program"
});

auto const RunCmd = Command::Add({
    .hotkey = "r",
    .buttonLabel = [](Debugger const&) { return "Run"; },
    .isActive = [](Debugger const& db) {
        return !db.model()->disassembly().empty();
    },
    .action = [](Debugger& db) { db.model()->startExecution(); },
    .description = "Run the currently loaded program"
});

auto const StopCmd = Command::Add({
    .hotkey = "x",
    .buttonLabel = [](Debugger const&) { return "Stop"; },
    .isActive = [](Debugger const& db) { return !db.model()->isIdle(); },
    .action = [](Debugger& db) { db.model()->stopExecution(); },
    .description = "Stop the currently running program"
});

auto const OpenCmd = Command::Add({
    .hotkey = "o",
    .buttonLabel = [](Debugger const&) { return "Open"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.openModal("file-open"); },
    .description = "Open an executable file for debugging"
});

auto const SettingsCmd = Command::Add({
    .hotkey = ",",
    .buttonLabel = [](Debugger const&) { return "Settings"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.openModal("settings"); },
    .description = "Show the settings window"
});

auto const HelpCmd = Command::Add({
    .hotkey = "h",
    .buttonLabel = [](Debugger const&) { return "Help"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.openModal("help"); },
    .description = "Show this help panel"
});

auto const ToggleLeftSidebarCmd = Command::Add({
    .hotkey = "L",
    .buttonLabel = [](Debugger const&) { return "⌷⎕"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.toggleSidebar(0); },
    .description = "Show or hide the left sidebar"
});

auto const ToggleRightSidebarCmd = Command::Add({
    .hotkey = "R",
    .buttonLabel = [](Debugger const&) { return "⎕⌷"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.toggleSidebar(1); },
    .description = "Show or hide the right sidebar"
});

auto const CycleMainViewCmd = Command::Add({
    .hotkey = "v",
    .buttonLabel = [](Debugger const& db) {
        switch (db.mainViewIndex()) {
        case 0:
            return "Src";
        case 1:
            return "S/A";
        case 2:
            return "Asm";
        default:
            return "???";
        }
    },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.cycleMainViews(); },
    .description = "Cycle the main views"
});

auto const ToggleConsoleCmd = Command::Add({
    .hotkey = "C",
    .buttonLabel = [](Debugger const&) { return "▂▂"; },
    .isActive = [](Debugger const&) { return true; },
    .action = [](Debugger& db) { db.toggleBottombar(); },
    .description = "Show or hide the console"
});

auto const ToggleExecCmd = Command::Add({
    .hotkey = "p",
    .buttonLabel = [](Debugger const& db) {
        return db.model()->isPaused() ? "|>" : "||";
    },
    .isActive = [](Debugger const& db) { return !db.model()->isIdle(); },
    .action = [](Debugger& db) { db.model()->toggleExecution(); },
    .description = "Toggle execution"
});

auto const StepInstCmd = Command::Add({
    .hotkey = "i",
    .buttonLabel = [](Debugger const&) { return ">."; },
    .isActive = [](Debugger const& db) {
        return db.model()->isPaused();
    },
    .action = [](Debugger& db) { db.model()->stepInstruction(); },
    .description = "Execute the current instruction)"
});

auto const StepSourceLineCmd = Command::Add({
    .hotkey = "l",
    .buttonLabel = [](Debugger const&) { return ">_"; },
    .isActive = [](Debugger const& db) {
        return !db.model()->sourceDebug().empty() && db.model()->isPaused();
    },
    .action = [](Debugger& db) { db.model()->stepSourceLine(); },
    .description = "Execute the current line"
});
// clang-format on

Debugger::Debugger():
    _screen(ScreenInteractive::Fullscreen()), _model(&uiHandle) {
    uiHandle.addRefreshCallback(
        [this] { _screen.PostEvent(Event::Special("Refresh")); });
    addModal("file-open", OpenFilePanel(model()));
    addModal("settings", SettingsView());
    addModal("help", HelpPanel());
    addModal("quit-confirm", QuitConfirm([this] { quit(); }));
    addModal("unload-confirm",
             UnloadConfirm([this] { model()->unloadProgram(); }));
    auto sidebar =
        TabView({ { " Files ", SourceFileBrowser(model(), uiHandle) },
                  { " VM State ", VMStateView(model()) } });
    auto sourceView = SourceView(model(), uiHandle);
    auto disasmView = DisassemblyView(model(), uiHandle);
    auto mainView = SplitLeft(sourceView, disasmView, &_mainSplitSize);
    auto dbgCtrlBar = Toolbar({
        ToolbarButton(this, ToggleExecCmd),
        ToolbarButton(this, StepSourceLineCmd),
        ToolbarButton(this, StepInstCmd),
        Spacer(),
        ToolbarButton(this, ToggleConsoleCmd),
    });
    auto bottom = Container::Vertical(
        { dbgCtrlBar, sdb::Separator(), ConsoleView(model()) });
    mainView = SplitBottom(bottom, mainView, &_bottombarSize);
    mainView = SplitLeft(sidebar, mainView, &_sidebarSize[0]);
    auto currentFileDisplay = Renderer(
        [this] { return text(model()->currentFilepath().string()) | flex; });
    auto toolbar = Toolbar({
        ToolbarButton(this, ToggleLeftSidebarCmd),
        ToolbarButton(this, QuitCmd),
        ToolbarButton(this, RunCmd),
        ToolbarButton(this, StopCmd),
        ToolbarButton(this, CycleMainViewCmd),
        sdb::Spacer(),
        currentFileDisplay,
        sdb::Spacer(),
        ToolbarButton(this, UnloadProgramCmd),
        ToolbarButton(this, OpenCmd),
        ToolbarButton(this, SettingsCmd),
        ToolbarButton(this, HelpCmd),
        ToolbarButton(this, ToggleRightSidebarCmd),
    });
    auto top = Container::Vertical({
        sdb::Separator(),
        toolbar,
        sdb::Separator(),
        mainView | flex,
    });
    root = top;
    root |= Command::EventCatcher(this);
    for (auto& [name, panel]: modalViews) {
        root |= panel.overlay();
    }
    // All unhandled key events generate a beep
    root |= CatchEvent([root = root.get()](Event event) {
        if (root->OnEvent(event)) return true;
        if (event.is_character()) beep();
        return false;
    });
    // Instruction view is focused by default
    disasmView->TakeFocus();
}

void Debugger::run() { _screen.Loop(root); }

void Debugger::quit() {
    model()->stopExecution();
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

void Debugger::cycleMainViews() {
    _mainViewIdx = (_mainViewIdx + 1) % 3;
    switch (_mainViewIdx) {
    case 0:
        _mainSplitSize = 1000;
        break;
    case 1:
        _mainSplitSize = _mainSplitSizeBackup;
        break;
    case 2:
        _mainSplitSizeBackup = _mainSplitSize;
        _mainSplitSize = -1;
        break;
    default:
        assert(false);
        break;
    }
}
