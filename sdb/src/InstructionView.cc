#include "Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model.h"
#include "ScrollBase.h"

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

struct InstView: ScrollBase {
    InstView(Model* model): model(model) {
        model->setScrollCallback([this](size_t index) {
            index = indexMap[index];
            if (!isInView(index)) {
                center(index);
            }
        });
        for (auto [index, inst]:
             model->instructions() | ranges::views::enumerate)
        {
            if (inst.labelID != 0) {
                Add(Renderer(
                    [ID = inst.labelID] { return text(labelName(ID)); }));
            }

            indexMap.push_back(ChildCount());
            ButtonOption opt = ButtonOption::Ascii();
            opt.transform = [=, index = index](EntryState const& s) {
                bool isCurrent = model->isActive() && model->isSleeping() &&
                                 index == model->currentLine();
                bool isBreakpoint = model->isBreakpoint(index);
                std::string labelText(toString(model->instructions()[index],
                                               &model->disassembly()));
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
    std::vector<size_t> indexMap;
};

} // namespace

Component sdb::InstructionView(Model* model) { return Make<InstView>(model); }
