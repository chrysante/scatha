#include "Views/Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model/Model.h"
#include "UI/Common.h"
#include "Views/FileViewCommon.h"
#include "Views/HelpPanel.h"

using namespace sdb;
using namespace ftxui;

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

struct InstView: FileViewBase<InstView> {
    InstView(Model* model, UIHandle& uiHandle): model(model) {
        uiHandle.addReloadCallback([this] { reload(); });
        uiHandle.addInstCallback([this](size_t index, BreakState state) {
            if (auto line = indexToLine(index)) {
                setFocusLine(*line);
                scrollToLine(*line);
            }
            breakIndex = index;
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
        return LineInfo{
            .num = lineNum,
            .isFocused = lineNum == focusLine(),
            .hasBreakpoint = index && model->hasInstBreakpoint(*index),
            .state = state,
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
            return lineMessageDecorator("Step Instruction") |
                   color(Color::White) | bgcolor(Color::Green);
        case Breakpoint:
            return lineMessageDecorator("Breakpoint") | color(Color::White) |
                   bgcolor(Color::Green);
        case Error:
            return lineMessageDecorator(error.message()) | color(Color::White) |
                   bgcolor(Color::RedLight);
        }
    }

    void reload() {
        DetachAllChildren();
        setFocusLine(0);
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

    bool OnEvent(Event event) override {
        if (FileViewBase::OnEvent(event)) {
            return true;
        }
        if (event == Event::Character("b")) {
            if (auto index = lineToIndex(focusLine())) {
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

    void toggleBreakpoint(size_t index) { model->toggleInstBreakpoint(index); }

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

    Model* model;
    svm::ErrorVariant error;
    std::atomic<std::optional<uint32_t>> breakIndex;
    std::atomic<BreakState> breakState = {};
    std::vector<long> indexToLineMap;
    utl::hashmap<long, size_t> lineToIndexMap;
};

} // namespace

ftxui::Component sdb::InstructionView(Model* model, UIHandle& uiHandle) {
    return Make<InstView>(model, uiHandle);
}
