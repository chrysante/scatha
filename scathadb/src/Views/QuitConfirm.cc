#include "Views/Views.h"

#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

static Component Button(std::string label, std::function<void()> action) {
    ButtonOption opt;
    opt.transform = [=](EntryState const&) { return text(label) | bold; };
    opt.on_click = action;
    return ftxui::Button(opt);
}

ModalView sdb::QuitConfirm(std::function<void()> doQuit) {
    auto state = ModalState::make();
    ToolbarOptions opt;
    opt.separator = spacer;
    opt.enclosingSeparators = true;
    auto cont = Toolbar(
                    {
                        ::Button("Quit", doQuit),
                        ::Button("Cancel", [=] { state->open = false; }),
                    },
                    opt) |
                CatchEvent([=](Event event) {
        if (event == Event::Character("q") || event == Event::Return) {
            doQuit();
            return true;
        }
        return false;
    });
    return ModalView("Confirm quit",
                     cont,
                     { .state = state, .closeButton = false });
}
