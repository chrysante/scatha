#ifndef SDB_VIEWS_H_
#define SDB_VIEWS_H_

#include <string>
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
ftxui::Component VMStateView(Model* model);

///
///
///
ftxui::Component ConsoleView(Model* model);

///
///
///
ftxui::Component Toolbar(std::vector<ftxui::Component> components);

/// Groups components with a name used by `TabView()`
struct NamedComponent {
    std::string name;
    ftxui::Component component;
};

///
///
///
ftxui::Component TabView(std::vector<NamedComponent> children);

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
