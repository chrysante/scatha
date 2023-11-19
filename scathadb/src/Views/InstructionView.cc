#include "Views/Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model/Model.h"
#include "UI/Common.h"
#include "Views/HelpPanel.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct LineInfo {
    long num;
    bool isFocused     : 1;
    bool hasBreakpoint : 1;
    BreakState state   : 3;
};

} // namespace

static Element breakpointIndicator(LineInfo line) {
    if (!line.hasBreakpoint) {
        return text("   ");
    }
    return text("-> ") |
           color(line.state != BreakState::None ? Color::White :
                                                  Color::BlueLight) |
           bold;
}

static Element lineNumber(LineInfo line) {
    return text(std::to_string(line.num + 1) + " ") | align_right |
           size(WIDTH, EQUAL, 5) |
           color(line.state != BreakState::None ? Color::White :
                                                  Color::GrayDark);
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
    InstView(Model* model, UIHandle& uiHandle): model(model) {
        uiHandle.addReloadCallback([this] { reload(); });
        uiHandle.addInstCallback([this](size_t index, BreakState state) {
            focusLine = indexToLine(index).value();
            breakIndex = index;
            scrollToIndex(index);
            breakState = state;
        });
        uiHandle.addResumeCallback([this]() {
            error = {};
            breakIndex = std::nullopt;
            breakState = {};
        });
        uiHandle.addErrorCallback(
            [this](svm::ErrorVariant err) { error = std::move(err); });
        reload();
    }

    LineInfo getLineInfo(long lineNum) const {
        auto index = lineToIndex(lineNum);
        BreakState state = [&] {
            using enum BreakState;
            auto breakIdx = breakIndex.load();
            if (!model->isPaused() || !index || !breakIdx) {
                return None;
            }
            if (*index != *breakIdx) {
                return None;
            }
            return breakState.load();
        }();
        return {
            .num = lineNum,
            .isFocused = lineNum == focusLine,
            .hasBreakpoint = index && model->hasInstBreakpoint(*index),
            .state = state,
        };
    };

    std::string makeErrorMessage() const {
        assert(error);
        // clang-format off
        return std::visit(utl::overload {
            [&](svm::MemoryAccessError const& err) {
                return utl::strcat("Bad access: ", err.pointer());
            },
            [&](svm::DeallocationError const& err) -> std::string {
                return "Bad deallocation";
            },
        }, *error); // clang-format on
    }

    ElementDecorator messageDecorator(std::string message) const {
        return [=](Element elem) {
            return hbox({ elem,
                          filler(),
                          hflow(paragraphAlignRight(std::move(message))) |
                              yflex_grow });
        };
    }

    ElementDecorator lineModifier(LineInfo line) const {
        using enum BreakState;
        switch (line.state) {
        case None:
            if (line.isFocused) {
                return color(Color::Black) | bgcolor(Color::GrayLight);
            }
            return nothing;
        case Step:
            return messageDecorator("Step Instruction") | color(Color::White) |
                   bgcolor(Color::Green);
        case Breakpoint:
            return messageDecorator("Breakpoint") | color(Color::White) |
                   bgcolor(Color::Green);
        case Error:
            return messageDecorator(makeErrorMessage()) | color(Color::White) |
                   bgcolor(Color::RedLight);
        }
    };

    void reload() {
        DetachAllChildren();
        focusLine = 0;
        indexToLineMap.clear();
        lineToIndexMap.clear();
        for (auto [index, inst]:
             model->disassembly().instructions() | ranges::views::enumerate)
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
                auto& disasm = model->disassembly();
                std::string labelText(
                    toString(disasm.instruction(index), &disasm, &model->VM()));
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
        if (event == Event::Special("Refresh")) {
            return true;
        }
        if (event == Event::Special("Reload")) {
            reload();
            return true;
        }
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
                model->toggleInstBreakpoint(*index);
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
                model->toggleInstBreakpoint(*index);
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
    std::optional<svm::ErrorVariant> error;
    std::atomic<long> focusLine = 0;
    std::atomic<std::optional<uint32_t>> breakIndex;
    std::atomic<BreakState> breakState = {};
    std::vector<long> indexToLineMap;
    utl::hashmap<long, size_t> lineToIndexMap;
};

} // namespace

View sdb::InstructionView(Model* model, UIHandle& uiHandle) {
    return Make<InstView>(model, uiHandle);
}
