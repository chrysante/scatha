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

struct SourceViewBase: FileViewBase<SourceViewBase> {
    SourceViewBase(Model* model, UIHandle& uiHandle): model(model) {
        uiHandle.addReloadCallback([this] { reload(); });
        uiHandle.addOpenSourceFileCallback([this](SourceFile const* file) {
            reload(file);
            TakeFocus();
        });
        uiHandle.addSourceCallback([this](size_t index, BreakState state) {
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

    Element Render() override {
        if (!file) {
            return placeholder("No File Open");
        }
        return ScrollBase::Render();
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
            .isFocused = lineNum == focusLine(),
            .hasBreakpoint = index && model->hasSourceBreakpoint(*index),
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

    void reload(SourceFile const* file = nullptr) {
        DetachAllChildren();
        if (!file) {
            auto& debug = model->sourceDebug();
            if (debug.empty()) {
                this->file = nullptr;
            }
            file = &debug.files().front();
        }
        this->file = file;
        for (auto [index, line]: file->lines() | ranges::views::enumerate) {
            Add(Renderer([=, index = index, line = std::string(line)] {
                auto lineInfo = getLineInfo(utl::narrow_cast<ssize_t>(index));
                return hbox({ lineNumber(lineInfo),
                              breakpointIndicator(lineInfo),
                              text(line) | flex }) |
                       lineModifier(lineInfo);
            }));
        }
    }

    void toggleBreakpoint(size_t index) {
        model->toggleSourceBreakpoint(index);
    }

    std::optional<size_t> lineToIndex(long line) const {
        if (line >= 0 && line < file->lines().size()) {
            return static_cast<size_t>(line) + 1;
        }
        return std::nullopt;
    }

    std::optional<long> indexToLine(size_t index) const {
        if (index < file->lines().size()) {
            return static_cast<long>(index) - 1;
        }
        return std::nullopt;
    }

    Model* model = nullptr;
    SourceFile const* file = nullptr;
    std::optional<svm::ErrorVariant> error;
    std::atomic<std::optional<uint32_t>> breakIndex;
    std::atomic<BreakState> breakState = {};
};

} // namespace

ftxui::Component sdb::SourceView(Model* model, UIHandle& uiHandle) {
    return Make<SourceViewBase>(model, uiHandle);
}
