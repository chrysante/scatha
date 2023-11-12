#ifndef SDB_VIEWS_VIEWS_H_
#define SDB_VIEWS_VIEWS_H_

#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "UI/Common.h"
#include "UI/ModalView.h"

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
ftxui::Component VMStateView(Model* model);

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

#endif // SDB_VIEWS_VIEWS_H_
