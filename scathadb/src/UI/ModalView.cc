#include "UI/ModalView.h"

using namespace sdb;
using namespace ftxui;

#include "UI/Common.h"

namespace {

struct ModalViewBase: ComponentBase {
    ModalViewBase(std::string title, Component body, bool* open): open(open) {
        auto closeOpt = ButtonOption::Simple();
        closeOpt.transform = [](EntryState const&) {
            return text(" X ") | bold;
        };
        closeOpt.on_click = [=] { *open = false; };
        auto titlebar = Container::Stacked({
            Renderer([=] { return text(title) | bold | center | flex; }),
            Container::Horizontal({ Button(closeOpt), sdb::separator() }),
        });
        Add(titlebar);
        Add(body);
    }

    Element Render() override {
        return vbox({
                   ChildAt(0)->Render(),
                   sdb::separator()->Render(),
                   ChildAt(1)->Render(),
               }) |
               size(WIDTH, GREATER_THAN, 30) | borderStyled(Color::GrayDark);
    }

    bool OnEvent(Event event) override {
        if (event == Event::Escape) {
            *open = false;
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

    Component ActiveChild() override { return ChildAt(1); }

    bool* open = nullptr;
};

} // namespace

ModalView::ModalView(std::string title,
                     Component body,
                     std::shared_ptr<State> state) {
    if (!state) {
        state = makeState();
    }
    _state = state;
    _comp = Make<ModalViewBase>(title, body, &_state->open);
}

void ModalView::open() {
    _state->open = true;
    _comp->TakeFocus();
}

OpenModalCommand ModalView::openCommand() {
    return [this] { open(); };
}

ComponentDecorator ModalView::overlay() {
    return ftxui::Modal(_comp, &_state->open);
}
