#ifndef SDB_DEBUGGER_H_
#define SDB_DEBUGGER_H_

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

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
    ftxui::Component settings;

    int regViewSize = 20;
    int consoleViewSize = 10;
    int memViewSize;
    bool showSettings = false;

    std::stringstream standardout;
};

} // namespace sdb

#endif // SDB_DEBUGGER_H_
