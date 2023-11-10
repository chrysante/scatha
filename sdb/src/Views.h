#ifndef SDB_VIEWS_H_
#define SDB_VIEWS_H_

#include <functional>

#include <ftxui/component/component.hpp>

namespace sdb {

class Model;

///
///
///
ftxui::Component ControlView(Model* model, std::function<void()> showSettings);

///
///
///
ftxui::Component InstructionView(Model* model);

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
ftxui::Component SettingsView(std::function<void()> hideSettings);

} // namespace sdb

#endif // SDB_VIEWS_H_
