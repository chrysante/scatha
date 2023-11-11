#ifndef SDB_VIEWS_H_
#define SDB_VIEWS_H_

#include <functional>

#include <ftxui/component/component.hpp>

#include "Common.h"
#include "ModalView.h"

namespace sdb {

class Model;

///
///
///
ftxui::Component ToolbarView(Model* model,
                             OpenModalCommand settings,
                             OpenModalCommand fileOpenPanel);

///
///
///
ftxui::Component StepControlsView(Model* model);

///
///
///
View InstructionView(Model* model);

///
///
///
ftxui::Component RegisterView(Model* model);

///
///
///
ftxui::Component FlagsView(Model* model);

///
///
///
ftxui::Component ConsoleView(Model* model);

///
///
///
ModalView OpenFilePanel(Model* model);

///
///
///
ModalView SettingsView();

} // namespace sdb

#endif // SDB_VIEWS_H_
