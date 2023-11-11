#include "Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

static Element breakpointIndicator(bool isBreakpoint, bool isCurrent) {
    if (isBreakpoint) {
        return text("*> ") |
               color(isCurrent ? Color::White : Color::BlueLight) | bold;
    }
    else {
        return text("   ");
    }
}

static Element lineNumber(size_t index, bool isCurrent) {
    return text(std::to_string(index + 1) + " ") | align_right |
           size(WIDTH, EQUAL, 5) |
           color(isCurrent ? Color::White : Color::GrayDark);
}

namespace {

struct InstView: ScrollBase {
    InstView(Model* model): model(model) { InstView::refresh(); }

    void refresh() override {
        DetachAllChildren();
        model->setScrollCallback([this](size_t index) {
            size_t line = indexToLineMap[index];
            if (!isInView(line)) {
                center(line);
            }
        });
        for (auto [index, inst]:
             model->instructions() | ranges::views::enumerate)
        {
            /// Add label renderer for labelled instructions
            if (inst.labelID != 0) {
                size_t lineNum = ChildCount();
                std::string name = utl::strcat(labelName(inst.labelID), ":");
                Add(Renderer([=] {
                    return hbox(
                        { lineNumber(lineNum, false), text(name) | bold });
                }));
            }
            /// Add instruction renderer
            size_t lineNum = ChildCount();
            indexToLineMap.push_back(lineNum);
            lineToIndexMap[lineNum] = index;
            Add(Renderer([=, index = index]() {
                bool isCurrent = model->isActive() && model->isSleeping() &&
                                 index == model->currentLine();
                bool isBreakpoint = model->isBreakpoint(index);
                std::string labelText(toString(model->instructions()[index],
                                               &model->disassembly()));
                auto line = hbox({ lineNumber(lineNum, isCurrent),
                                   breakpointIndicator(isBreakpoint, isCurrent),
                                   text(labelText) | flex });
                if (isCurrent) {
                    line |= color(Color::White);
                    line |= bgcolor(Color::Green);
                }
                return line;
            }));
        }
    }

    bool OnEvent(Event event) override {
        if (handleScroll(event)) {
            return true;
        }
        /// Custom button behaviour because the builtin button does not work
        /// correctly with the scroll view
        if (event.is_mouse() &&
            box().Contain(event.mouse().x, event.mouse().y) &&
            event.mouse().motion == Mouse::Released)
        {
            auto line = event.mouse().y - box().y_min + scrollPosition();
            auto itr = lineToIndexMap.find(line);
            if (itr != lineToIndexMap.end()) {
                size_t index = itr->second;
                model->toggleBreakpoint(index);
            }
            return true;
        }
        return false;
    }

    Model* model;
    std::vector<size_t> indexToLineMap;
    utl::hashmap<size_t, size_t> lineToIndexMap;
};

} // namespace

View sdb::InstructionView(Model* model) { return Make<InstView>(model); }
