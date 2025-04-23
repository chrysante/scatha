#include "Views/Views.h"

#include <svm/Exceptions.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model/Events.h"
#include "UI/Common.h"
#include "Util/Messenger.h"

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

// We put this here for now
ModalView sdb::PatientStartFailureModal(std::shared_ptr<Messenger> messenger) {
    struct Impl: ComponentBase, Transceiver {
        Impl(std::shared_ptr<Messenger> messenger):
            Transceiver(std::move(messenger)) {
            listen([this](PatientStartFailureEvent const& event) {
                exc = event.exception;
            });
        }

        Element OnRender() override {
            return text(exc.message()) | center | flex;
        }

        svm::ExceptionVariant exc;
    };
    return ModalView("Error", Make<Impl>(std::move(messenger)));
}
