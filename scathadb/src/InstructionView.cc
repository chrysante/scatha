#include "Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Common.h"
#include "HelpPanel.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct LineInfo {
    long num          : 60;
    bool isFocused    : 1;
    bool isCurrent    : 1;
    bool isBreakpoint : 1;
};

} // namespace

static Element breakpointIndicator(LineInfo line) {
    if (!line.isBreakpoint) {
        return text("   ");
    }
    return text("*> ") |
           color(line.isCurrent ? Color::White : Color::BlueLight) | bold;
}

static Element lineNumber(LineInfo line) {
    return text(std::to_string(line.num + 1) + " ") | align_right |
           size(WIDTH, EQUAL, 5) |
           color(line.isCurrent ? Color::White : Color::GrayDark);
}

[[maybe_unused]] static int desc_init = [] {
    setPanelCommandsInfo("Instruction viewer commands",
                         {
                             { "b",
                               "Create or erase a breakpoint on the current "
                               "line" },
                             { "c", "Clear all breakpoints" },
                         });
    return 0;
}();

namespace {

struct InstView: ScrollBase {
    InstView(Model* model): model(model) { InstView::refresh(); }

    LineInfo getLineInfo(long lineNum) const {
        auto index = lineToIndex(lineNum);
        return { .num = lineNum,
                 .isFocused = lineNum == focusLine,
                 .isCurrent = model->isActive() && model->isSleeping() &&
                              index && *index == model->currentLine(),
                 .isBreakpoint = index && model->isBreakpoint(*index) };
    };

    ElementDecorator lineModifier(LineInfo line) const {
        if (line.isCurrent) {
            return color(Color::White) | bgcolor(Color::Green);
        }
        if (line.isFocused) {
            return color(Color::Black) | bgcolor(Color::GrayLight);
        }
        return nothing;
    };

    void refresh() override {
        DetachAllChildren();
        focusLine = 0;
        indexToLineMap.clear();
        lineToIndexMap.clear();

        model->setScrollCallback([this](size_t index) {
            focusLine = indexToLine(index).value();
            scrollToIndex(index);
        });

        for (auto [index, inst]:
             model->instructions() | ranges::views::enumerate)
        {
            /// Add label renderer for labelled instructions
            if (inst.labelID != 0) {
                long lineNum = static_cast<long>(ChildCount());
                std::string name = utl::strcat(labelName(inst.labelID), ":");
                Add(Renderer([=] {
                    auto line = getLineInfo(lineNum);
                    return hbox(
                               { lineNumber(line), text(name) | bold | flex }) |
                           lineModifier(line);
                }));
            }
            /// Add instruction renderer
            long lineNum = static_cast<long>(ChildCount());
            indexToLineMap.push_back(lineNum);
            lineToIndexMap[lineNum] = index;
            Add(Renderer([=, index = index] {
                auto line = getLineInfo(lineNum);
                std::string labelText(toString(model->instructions()[index],
                                               &model->disassembly()));
                return hbox({ lineNumber(line),
                              breakpointIndicator(line),
                              text(labelText) | flex }) |
                       lineModifier(line);
            }));
        }
    }

    Element Render() override {
        if (model->disassembly().empty()) {
            return placeholder("No Program Loaded");
        }
        return ScrollBase::Render();
    }

    bool Focusable() const override { return true; }

    bool OnEvent(Event event) override {
        if (handleScroll(event, /* allowKeyScroll = */ false)) {
            return true;
        }
        /// Custom button behaviour because the builtin button does not work
        /// correctly with the scroll view
        if (event.is_mouse()) {
            return handleMouse(event);
        }
        if (event == Event::ArrowUp) {
            focusLineOffset(-1);
            return true;
        }
        if (event == Event::ArrowDown) {
            focusLineOffset(1);
            return true;
        }
        /// We eat these to prevent focus loss
        if (event == Event::ArrowLeft || event == Event::ArrowRight) {
            return true;
        }
        if (event == Event::Character("b")) {
            if (auto index = lineToIndex(focusLine)) {
                model->toggleBreakpoint(*index);
            }
            else {
                beep();
            }
            return true;
        }
        if (event == Event::Character("c")) {
            model->clearBreakpoints();
            return true;
        }
        return false;
    }

    bool handleMouse(Event event) {
        auto mouse = event.mouse();
        if (!box().Contain(mouse.x, mouse.y)) {
            return false;
        }
        if (mouse.button != Mouse::None && mouse.motion == Mouse::Pressed) {
            TakeFocus();
        }
        if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed) {
            long line = event.mouse().y - box().y_min + scrollPosition();
            long column = event.mouse().x - box().x_min;
            auto index = lineToIndex(line);
            if (index && column < 8) {
                model->toggleBreakpoint(*index);
            }
            else {
                focusLine = event.mouse().y - box().y_min + scrollPosition();
            }
            return true;
        }
        return false;
    }

    void scrollToIndex(size_t index) { scrollToLine(indexToLineMap[index]); }

    void scrollToLine(long line) {
        if (isInView(line)) {
            return;
        }
        if (line < scrollPosition()) {
            center(line, 0.25);
        }
        else {
            center(line, 0.75);
        }
    }

    std::optional<size_t> lineToIndex(long line) const {
        auto itr = lineToIndexMap.find(line);
        if (itr != lineToIndexMap.end()) {
            return itr->second;
        }
        return std::nullopt;
    }

    std::optional<long> indexToLine(size_t index) const {
        if (index < indexToLineMap.size()) {
            return indexToLineMap[index];
        }
        return std::nullopt;
    }

    void focusLineOffset(long offset) {
        focusLine = std::clamp(focusLine + offset,
                               0l,
                               utl::narrow_cast<long>(ChildCount()) - 1);
        scrollToLine(focusLine);
    }

    Model* model;
    long focusLine = 0;
    std::vector<long> indexToLineMap;
    utl::hashmap<long, size_t> lineToIndexMap;
};

} // namespace

View sdb::InstructionView(Model* model) { return Make<InstView>(model); }
