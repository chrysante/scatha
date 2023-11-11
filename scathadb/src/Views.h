#ifndef SDB_VIEWS_H_
#define SDB_VIEWS_H_

#include <vector>

#include <ftxui/component/component.hpp>

#include "Common.h"
#include "ModalView.h"

namespace sdb {

class Model;
class Debugger;

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
ftxui::Component Toolbar(std::vector<ftxui::Component> components);

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
