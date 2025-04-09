#ifndef SVM_VIEWS_HELPPANEL_H_
#define SVM_VIEWS_HELPPANEL_H_

#include <string>
#include <vector>

#include "UI/ModalView.h"

namespace sdb {

/// Groups information about hotkey commands
struct CommandInfo {
    /// The hotkey to execute this command
    std::string hotkey;

    /// A description of what this command does
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
