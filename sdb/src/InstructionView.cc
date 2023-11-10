#include "Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "Model.h"

using namespace sdb;
using namespace ftxui;

static Element breakpointIndicator(bool isBreakpoint) {
    if (isBreakpoint) {
        return text("> ") | color(Color::BlueLight) | bold;
    }
    else {
        return text("  ");
    }
}

static Element lineNumber(size_t index, bool isCurrent) {
    return text(std::to_string(index + 1) + " ") | align_right |
           size(WIDTH, EQUAL, 5) |
           color(isCurrent ? Color::White : Color::GrayLight);
}

namespace {

struct InstView: ComponentBase {
    InstView(Model* model): model(model) {
        for (auto [index, inst]:
             model->instructions() | ranges::views::enumerate)
        {
            ButtonOption opt = ButtonOption::Ascii();
            opt.transform = [=, index = index](EntryState const& s) {
                bool isCurrent = model->isActive() && model->isSleeping() &&
                                 index == model->currentLine();
                bool isBreakpoint = model->isBreakpoint(index);
                std::string labelText(toString(model->instructions()[index]));
                auto label =
                    hbox({ lineNumber(index, isCurrent), text(labelText) }) |
                    flex;
                if (isCurrent) {
                    label |= bgcolor(Color::Green);
                }
                return hbox({ breakpointIndicator(isBreakpoint), label });
            };
            opt.on_click = [=, index = index] {
                model->toggleBreakpoint(index);
            };
            Add(Button(opt));
        }
    }

    Model* model;
};

} // namespace

Component sdb::InstructionView(Model* model) {
    return ScrollView(Make<InstView>(model));
}
