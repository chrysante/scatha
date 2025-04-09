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

namespace {

struct ConfirmModalOptions {
    std::string title;
    std::string confirmLabel;
    std::string confirmHotkey;
};

} // namespace

static ModalView ConfirmImpl(std::function<void()> action,
                             ConfirmModalOptions const& options) {
    auto state = ModalState::make();
    ToolbarOptions opt;
    opt.separator = spacer;
    opt.enclosingSeparators = true;
    auto cont = Toolbar(
                    {
                        ::Button(options.confirmLabel,
                                 [=] {
        action();
        state->open = false;
    }),
                        ::Button("Cancel", [=] { state->open = false; }),
                    },
                    opt) |
                CatchEvent([=](Event event) {
        if (event == Event::Character(options.confirmHotkey) ||
            event == Event::Return)
        {
            action();
            state->open = false;
            return true;
        }
        return false;
    });
    return ModalView(options.title, cont,
                     { .state = state, .closeButton = false });
}

ModalView sdb::QuitConfirm(std::function<void()> doQuit) {
    return ConfirmImpl(std::move(doQuit), {
                                              .title = "Confirm Quit",
                                              .confirmLabel = "Quit",
                                              .confirmHotkey = "q",
                                          });
}

ModalView sdb::UnloadConfirm(std::function<void()> doUnload) {
    return ConfirmImpl(std::move(doUnload), {
                                                .title = "Confirm Unload",
                                                .confirmLabel = "Unload",
                                                .confirmHotkey = "u",
                                            });
}
