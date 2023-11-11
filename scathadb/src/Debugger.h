#ifndef SDB_DEBUGGER_H_
#define SDB_DEBUGGER_H_

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

#include "Common.h"

namespace sdb {

class Model;

struct KeyCommand {
    std::string key;
    std::function<void()> command;
};

class Debugger {
public:
    explicit Debugger(Model* model);

    void run();

private:
    void addKeyCommand(std::string key, std::function<void()> command);

    Model* model;
    std::vector<KeyCommand> keyCommands;

    ftxui::ScreenInteractive screen;
    ftxui::Component root;
    std::shared_ptr<ViewBase> instView;

    bool fileOpenPanelOpen = false;
    ftxui::Component fileOpenPanel;
    bool settingsOpen = false;
    ftxui::Component settings;
};

} // namespace sdb

#endif // SDB_DEBUGGER_H_
