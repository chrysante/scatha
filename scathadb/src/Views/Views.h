#ifndef SDB_VIEWS_VIEWS_H_
#define SDB_VIEWS_VIEWS_H_

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "UI/Common.h"
#include "UI/ModalView.h"

namespace sdb {

class Model;

/// Displays the disassembled instructions of the currently loaded program as
/// well as execution stepping and allows settings breakpoints
View InstructionView(Model* model);

/// Displays VM state like registers and compare flags
ftxui::Component VMStateView(Model* model);

/// Displays stdout output of the running program
ftxui::Component ConsoleView(Model* model);

/// Displays a file dialogue to load programs into the debugger
ModalView OpenFilePanel(Model* model);

/// Display debugger settings
ModalView SettingsView();

/// Confirm quitting the app
ModalView QuitConfirm(std::function<void()> doQuit);

} // namespace sdb

#endif // SDB_VIEWS_VIEWS_H_
