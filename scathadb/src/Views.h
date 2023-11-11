#ifndef SDB_VIEWS_H_
#define SDB_VIEWS_H_

#include <functional>

#include <ftxui/component/component.hpp>

#include "Common.h"

namespace sdb {

class Model;

///
///
///
ftxui::Component ToolbarView(Model* model,
                             bool* settingsOpen,
                             bool* filePanelOpen);

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
ftxui::Component OpenFilePanel(Model* model, bool* open);

///
///
///
ftxui::Component SettingsView(bool* open);

} // namespace sdb

#endif // SDB_VIEWS_H_
