#ifndef SDB_DEBUGGER_H_
#define SDB_DEBUGGER_H_

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

#include "Common.h"
#include "ModalView.h"

namespace sdb {

class Model;

struct KeyCommand {
    std::string key;
    std::function<void()> command;
};

///
///
class Debugger {
public:
    explicit Debugger(Model* model);

    ///
    void run();

    ///
    Model* model() { return _model; }

    /// \overload
    Model const* model() const { return _model; }

private:
    void addKeyCommand(std::string key, std::function<void()> command);
    Model* _model;

public:
    // Public for now
    ModalView fileOpenPanel;
    ModalView settingsView;
    ftxui::ScreenInteractive screen;

private:
    std::vector<KeyCommand> keyCommands;
    ftxui::Component root;
    std::shared_ptr<ViewBase> instView;
};

} // namespace sdb

#endif // SDB_DEBUGGER_H_
