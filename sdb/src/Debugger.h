#ifndef SDB_DEBUGGER_H_
#define SDB_DEBUGGER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

namespace sdb {

class Program;

struct KeyCommand {
    std::string key;
    std::function<void()> command;
};

class Debugger {
public:
    explicit Debugger(Program* program);

    void run();

private:
    void addKeyCommand(std::string key, std::function<void()> command);

    std::vector<KeyCommand> keyCommands;

    Program* prog;

    ftxui::ScreenInteractive screen;
    ftxui::Component root;

    int regViewSize;
    int memViewSize;
};

} // namespace sdb

#endif // SDB_DEBUGGER_H_
