#ifndef SDB_VIEWS_VIEWS_H_
#define SDB_VIEWS_VIEWS_H_

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "UI/Common.h"
#include "UI/ModalView.h"

namespace sdb {

class Messenger;
class Model;

/// Displays the disassembled instructions of the currently loaded program as
/// well as execution stepping and allows settings breakpoints
ftxui::Component DisassemblyView(Model* model,
                                 std::shared_ptr<Messenger> messenger);

/// Displays the source code of the currently loaded program as
/// well as execution stepping and allows settings breakpoints
ftxui::Component SourceView(Model* model, std::shared_ptr<Messenger> messenger);

/// Displays a file browser for the source files of the currently loaded program
ftxui::Component SourceFileBrowser(Model* model,
                                   std::shared_ptr<Messenger> messenger);

/// Displays VM state like registers and compare flags
ftxui::Component VMStateView(Model* model);

/// Displays stdout output of the running program
ftxui::Component ConsoleView(Model* model);

/// Displays a file dialogue to load programs into the debugger
ModalView OpenFilePanel(Model* model);

/// Display debugger settings
ModalView SettingsView();

/// Displays an error message regarding why the patient program could not start
ModalView PatientStartFailureModal(std::shared_ptr<Messenger> messenger);

/// Confirm quitting the app
ModalView QuitConfirm(std::function<void()> doQuit);

/// Confirm unloading the current program
ModalView UnloadConfirm(std::function<void()> doUnload);

} // namespace sdb

#endif // SDB_VIEWS_VIEWS_H_
