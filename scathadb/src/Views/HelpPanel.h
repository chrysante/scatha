#ifndef SVM_VIEWS_HELPPANEL_H_
#define SVM_VIEWS_HELPPANEL_H_

#include <string>
#include <vector>

#include "UI/ModalView.h"

namespace sdb {

///
struct CommandInfo {
    ///
    std::string hotkey;

    ///
    std::string message;
};

///
using PanelCommandsInfo = std::vector<CommandInfo>;

///
void addPanelCommandsInfo(std::string panelName, CommandInfo info);

///
void setPanelCommandsInfo(std::string panelName, PanelCommandsInfo info);

///
ModalView HelpPanel();

} // namespace sdb

#endif // SVM_VIEWS_HELPPANEL_H_
