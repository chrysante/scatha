#include "Views/Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model/Model.h"
#include "Model/SourceFile.h"
#include "UI/Common.h"
#include "Views/FileViewCommon.h"
#include "Views/HelpPanel.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct SourceViewBase: FileViewBase {
    SourceViewBase(Model* model, UIHandle& uiHandle): model(model) {
        uiHandle.addReloadCallback([this] { reload(); });
        uiHandle.addOpenSourceFileCallback([this](size_t index) {
            reloadFile(index);
            TakeFocus();
        });
        uiHandle.addEncounterCallback(
            [this](scdis::InstructionPointerOffset ipo, BreakState state) {
            auto SL = this->model->sourceDebug().sourceMap().toSourceLoc(ipo);
            if (!SL) return;
            auto [fileIdx, lineIdx] = SL->line;
            if (lineIdx != fileIndex) reloadFile(size_t(fileIdx));
            if (auto line = indexToLine(lineIdx)) {
                setFocusLine(*line);
                scrollToLine(*line);
            }
            breakIndex = lineIdx;
            breakState = state;
        });
        uiHandle.addResumeCallback([this]() {
            exc = {};
            breakIndex = std::nullopt;
            breakState = {};
        });
        uiHandle.addExceptionCallback(
            [this](svm::ExceptionVariant exc) { this->exc = std::move(exc); });

        reload();
    }

    Element Render() override {
        if (model->disassembly().empty())
            return placeholder("No Program Loaded");
        if (!fileIndex) return placeholder("No File Open");
        return ScrollBase::Render();
    }

    bool OnEvent(Event event) override { return FileViewBase::OnEvent(event); }

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
        SourceLine sourceLine = { (uint32_t)*fileIndex, (uint32_t)*index };
        return LineInfo{
            .lineIndex = lineNum,
            .isFocused = lineNum == focusLine(),
            .hasBreakpoint = index && model->hasSourceBreakpoint(sourceLine),
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
        case Paused:
            return lineMessageDecorator("Paused") | color(Color::White) |
                   bgcolor(Color::Green);
        case Step:
            return lineMessageDecorator("Step") | color(Color::White) |
                   bgcolor(Color::Green);
        case Breakpoint:
            return lineMessageDecorator("Breakpoint") | color(Color::White) |
                   bgcolor(Color::Green);
        case Error:
            return lineMessageDecorator("Error") | color(Color::White) |
                   bgcolor(Color::RedLight);
        }
    }

    void clearBreakpoints() override {}

    void reload() override { reloadImpl(std::nullopt); }

    void reloadFile(size_t fileIndex) { reloadImpl(fileIndex); }

    void reloadImpl(std::optional<size_t> sourceFileIndex) {
        DetachAllChildren();
        auto& debug = model->sourceDebug();
        if (!sourceFileIndex && !debug.empty()) {
            sourceFileIndex = 0;
        }
        fileIndex = sourceFileIndex;
        if (!fileIndex) {
            return;
        }
        auto& file = debug.files()[*fileIndex];
        for (auto [index, line]: file.lines() | ranges::views::enumerate) {
            auto component =
                Renderer([this, index = index, line = std::string(line)] {
                auto lineInfo = getLineInfo(utl::narrow_cast<ssize_t>(index));
                return hbox({ lineNumber(lineInfo),
                              breakpointIndicator(lineInfo),
                              text(line) | flex }) |
                       lineModifier(lineInfo);
            });
            SourceLine sourceLine{ utl::narrow_cast<uint32_t>(*sourceFileIndex),
                                   utl::narrow_cast<uint32_t>(index) };
            component |= CatchEvent([this, sourceLine](Event event) {
                if (event == Event::Character('b') &&
                    sourceLine.line == (size_t)focusLine() + 1)
                {
                    if (!model->toggleSourceBreakpoint(sourceLine)) beep();
                    return true;
                }
                return false;
            });
            Add(component);
        }
    }

    void toggleBreakpoint(SourceLine line) {
        model->toggleSourceBreakpoint(line);
    }

    std::optional<size_t> lineToIndex(long line) const {
        auto& file = model->sourceDebug().files()[*fileIndex];
        if (line >= 0 && line < (ssize_t)file.lines().size()) {
            return static_cast<size_t>(line) + 1;
        }
        return std::nullopt;
    }

    std::optional<long> indexToLine(size_t index) const {
        auto& file = model->sourceDebug().files()[*fileIndex];
        if (index < file.lines().size()) {
            return static_cast<long>(index) - 1;
        }
        return std::nullopt;
    }

    Model* model = nullptr;
    std::optional<size_t> fileIndex = 0;
    std::optional<svm::ExceptionVariant> exc;
    std::atomic<std::optional<uint32_t>> breakIndex;
    std::atomic<BreakState> breakState = {};
};

} // namespace

ftxui::Component sdb::SourceView(Model* model, UIHandle& uiHandle) {
    return Make<SourceViewBase>(model, uiHandle);
}
