#include "UI/ModalView.h"

using namespace sdb;
using namespace ftxui;

#include "UI/Common.h"

namespace {

struct ModalViewBase: ComponentBase {
    ModalViewBase(std::string title, Component body, ModalOptions options,
                  bool* open):
        open(open) {
        auto titlebar = [&] {
            auto bar = Container::Stacked({});
            if (options.closeButton) {
                auto closeOpt = ButtonOption::Simple();
                closeOpt.transform = [](EntryState const&) {
                    return text(" X ") | bold;
                };
                closeOpt.on_click = [=] { *open = false; };
                bar->Add(Container::Horizontal(
                    { Button(closeOpt), sdb::Separator() }));
            }
            bar->Add(Renderer([=] { return text(title) | center | flex; }));
            return bar;
        }();
        Add(titlebar);
        Add(body);
    }

    Element OnRender() override {
        return vbox({
                   ChildAt(0)->Render(),
                   sdb::separator(),
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

ModalView::ModalView(std::string title, Component body, ModalOptions options) {
    if (!options.state) {
        options.state = ModalState::make();
    }
    _state = std::move(options.state);
    _comp = Make<ModalViewBase>(title, body, options, &_state->open);
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
