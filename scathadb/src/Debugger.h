#ifndef SDB_DEBUGGER_H_
#define SDB_DEBUGGER_H_

#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

#include "Common.h"
#include "ModalView.h"

namespace sdb {

class Model;

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

private:
    ftxui::ScreenInteractive screen;

    Model* _model;
    std::unordered_map<std::string, ModalView> modalViews;
    ftxui::Component root;
    std::shared_ptr<ViewBase> instView;
};

} // namespace sdb

#endif // SDB_DEBUGGER_H_
