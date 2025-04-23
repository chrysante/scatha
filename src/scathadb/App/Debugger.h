#ifndef SDB_APP_DEBUGGER_H_
#define SDB_APP_DEBUGGER_H_

#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

#include "App/Messenger.h"
#include "Model/Model.h"
#include "UI/Common.h"
#include "UI/ModalView.h"

namespace sdb {

class Model;
class Debugger;

///
///
class Debugger {
public:
    Debugger();

    ///
    void run();

    ///
    Model* model() { return &_model; }

    /// \overload
    Model const* model() const { return &_model; }

    ///
    void quit();

    ///
    ModalView* getModal(std::string name) {
        return const_cast<ModalView*>(std::as_const(*this).getModal(name));
    }

    /// \overload
    ModalView const* getModal(std::string name) const {
        auto itr = modalViews.find(name);
        if (itr != modalViews.end()) {
            return &itr->second;
        }
        return nullptr;
    }

    ///
    void openModal(std::string name) {
        auto* mod = getModal(name);
        if (mod) {
            mod->open();
        }
    }

    ///
    bool addModal(std::string name, ModalView modal) {
        auto [itr, success] = modalViews.insert({ name, modal });
        return success;
    }

    ///
    void toggleSidebar(size_t index);

    ///
    void toggleBottombar();

    ///
    int mainViewIndex() const { return _mainViewIdx; }

    ///
    void cycleMainViews();

    ///
    ftxui::ScreenInteractive& screen() { return _screen; }

private:
    void refreshScreen();

    ftxui::ScreenInteractive _screen;

    std::shared_ptr<Messenger> _messenger;
    Model _model;
    std::unordered_map<std::string, ModalView> modalViews;
    ftxui::Component root;
    int _mainViewIdx = 0;
    int _mainSplitSize = 30;
    int _sidebarSize[2] = { 30, 30 };
    int _sidebarSizeBackup[2] = { 30, 30 };
    int _bottombarSize = 10;
    int _bottombarSizeBackup = 10;
};

} // namespace sdb

#endif // SDB_APP_DEBUGGER_H_
