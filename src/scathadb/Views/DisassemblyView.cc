#include "Views/Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <scbinutil/OpCode.h>
#include <scdis/Print.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "App/Messenger.h"
#include "Model/Events.h"
#include "Model/Model.h"
#include "UI/Common.h"
#include "Views/FileViewCommon.h"
#include "Views/HelpPanel.h"

using namespace sdb;
using namespace ftxui;

[[maybe_unused]] static int desc_init = [] {
    setPanelCommandsInfo(
        "Instruction viewer commands",
        {
            { "b", "Create or erase a breakpoint on the current line" },
            { "c", "Clear all breakpoints" },
        });
    return 0;
}();

namespace {

struct DisasmView: FileViewBase, Transceiver {
    DisasmView(Model* model, std::shared_ptr<Messenger> messenger);

    void reload();
    void clearBreakpoints() override { model->clearBreakpoints(); }

    LineInfo getInstLineInfo(long lineIndex, size_t instIndex) const;
    LineInfo getLabelLineInfo(long lineIndex) const;

    Element Render() override;
    bool OnEvent(Event event) override;

    void toggleBreakpoint(size_t index) { model->toggleInstBreakpoint(index); }

    friend void doSetFocusLine(DisasmView& This, long line) {
        This.setFocusLine(line);
    }

    Model* model;
    svm::ExceptionVariant exc;
    std::atomic<std::optional<uint32_t>> breakIndex;
    std::atomic<BreakState> breakState = {};
    std::vector<long> instIndexToLineMap;
};

} // namespace

ftxui::Component sdb::DisassemblyView(Model* model,
                                      std::shared_ptr<Messenger> messenger) {
    return Make<DisasmView>(model, std::move(messenger));
}

DisasmView::DisasmView(Model* mdl, std::shared_ptr<Messenger> messenger):
    Transceiver(messenger), model(mdl) {
    listen([this](ReloadUIRequest) { reload(); });
    listen([this](BreakEvent const& event) {
        auto instIndex = model->disassembly().indexMap().ipoToIndex(event.ipo);
        if (!instIndex) return;
        long lineIndex = instIndexToLineMap[*instIndex];
        setFocusLine(lineIndex);
        scrollToLine(lineIndex);
        breakIndex = *instIndex;
        breakState = event.state;
        exc = event.exception;
    });
    reload();
}

static auto labelDeco() { return bold | color(Color::GreenLight); }

static auto instDeco() {
    return size(WIDTH, GREATER_THAN, 6) | color(Color::BlueLight);
}

static auto registerDeco() { return bold | color(Color::MagentaLight); }

static auto immediateDeco() { return color(Color::CyanLight); }

static auto ffiDeco() { return color(Color::Yellow); }

static ElementDecorator lineModifier(LineInfo line,
                                     svm::ExceptionVariant const& exc) {
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
        return lineMessageDecorator("Step Instruction") | color(Color::White) |
               bgcolor(Color::Green);
    case Breakpoint:
        return lineMessageDecorator("Breakpoint") | color(Color::White) |
               bgcolor(Color::Green);
    case Error:
        return lineMessageDecorator(exc.message()) | color(Color::White) |
               bgcolor(Color::RedLight);
    }
}

namespace {

static bool handleMouseEventOnLine(
    DisasmView* This, Event event, long lineIdx,
    std::optional<size_t> instIdx = std::nullopt) {
    if (!event.is_mouse() || event.mouse().button != Mouse::Left ||
        event.mouse().motion != Mouse::Pressed)
        return false;
    auto box = This->box();
    long line = event.mouse().y - box.y_min + This->scrollPosition();
    if (line != lineIdx) return false;
    long column = event.mouse().x - box.x_min;
    if (instIdx && column < 8)
        This->toggleBreakpoint(*instIdx);
    else
        doSetFocusLine(*This, lineIdx);
    return true;
}

struct ConstructionDelegate final: scdis::PrintDelegate {
    DisasmView* This;

    long lineIndex = 0;
    size_t instIndex = 0;
    std::vector<Element> buf{};

    explicit ConstructionDelegate(DisasmView* This): This(This) {}

    void instName(std::string_view name) override {
        buf.push_back(text(std::string(name)) | instDeco());
    }
    void registerName(size_t index) override {
        buf.push_back(text(utl::strcat("%", index)) | registerDeco());
    }
    void immediate(uint64_t value) override {
        buf.push_back(text(std::to_string(value)) | immediateDeco());
    }
    void label(scdis::Label const& label) override {
        long lineIdx = lineIndex++;
        auto labelView = Renderer([=, This = This, name = label.name] {
            auto lineInfo = This->getLabelLineInfo(lineIdx);
            return hbox(
                       { lineNumber(lineInfo), text(name) | bold, text(":") }) |
                   lineModifier(lineInfo, This->exc);
        });
        labelView |= CatchEvent([=, This = This](Event event) {
            if (handleMouseEventOnLine(This, event, lineIdx)) return true;
            return false;
        });
        This->Add(labelView);
    }
    void labelName(scdis::Label const& label) override {
        buf.push_back(text(label.name) | labelDeco());
    }
    void ffiName(std::string_view name) override {
        buf.push_back(text(std::string(name)) | ffiDeco());
    }
    void string(std::string_view value) override {
        buf.push_back(text(utl::strcat("\"", value, "\"")) | color(Color::Red));
    }
    void plaintext(std::string_view str) override {
        buf.push_back(text(std::string(str)));
    }

    void beginInst(scdis::Instruction const&) override { buf.clear(); }
    void endInst() override {
        long lineIdx = lineIndex++;
        size_t instIdx = instIndex++;
        This->instIndexToLineMap.push_back(lineIdx);
        auto instView = Renderer([=, This = This, buf = std::move(buf)] {
            auto instElem = hbox(buf);
            auto lineInfo = This->getInstLineInfo(lineIdx, instIdx);
            return hbox({ lineNumber(lineInfo), breakpointIndicator(lineInfo),
                          instElem | flex }) |
                   lineModifier(lineInfo, This->exc);
        });
        instView |= CatchEvent([=, This = This](Event event) {
            if (event == Event::Character("b") && lineIdx == This->focusLine())
            {
                This->toggleBreakpoint(instIdx);
                return true;
            }
            if (handleMouseEventOnLine(This, event, lineIdx, instIdx))
                return true;
            return false;
        });
        This->Add(instView);
    }
    void beginData(scdis::InstructionPointerOffset) override {}
    void endData() override {
        long lineIdx = lineIndex++;
        auto dataView = Renderer([=, This = This, buf = std::move(buf)] {
            auto elem = hbox(buf);
            auto lineInfo = This->getLabelLineInfo(lineIdx);
            return hbox({ lineNumber(lineInfo), breakpointIndicator(lineInfo),
                          elem | flex }) |
                   lineModifier(lineInfo, This->exc);
        });
        dataView |= CatchEvent([=, This = This](Event event) {
            if (handleMouseEventOnLine(This, event, lineIdx)) return true;
            return false;
        });
        This->Add(dataView);
    }
};

} // namespace

static void constructInstList(scdis::Disassembly const& disasm,
                              DisasmView* This) {
    ConstructionDelegate del(This);
    scdis::print(disasm, del);
}

void DisasmView::reload() {
    DetachAllChildren();
    setFocusLine(0);
    instIndexToLineMap.clear();
    constructInstList(model->disassembly(), this);
}

LineInfo DisasmView::getInstLineInfo(long lineIndex, size_t instIndex) const {
    auto breakIdx = breakIndex.load();
    auto state = model->isPaused() && instIndex == breakIdx ?
                     breakState.load() :
                     BreakState::None;
    return {
        .lineIndex = lineIndex,
        .isFocused = lineIndex == focusLine(),
        .hasBreakpoint = model->hasInstBreakpoint(instIndex),
        .state = state,
    };
}

LineInfo DisasmView::getLabelLineInfo(long lineIndex) const {
    return {
        .lineIndex = lineIndex,
        .isFocused = lineIndex == focusLine(),
        .hasBreakpoint = false,
        .state = BreakState::None,
    };
}

Element DisasmView::Render() {
    if (model->disassembly().empty()) return placeholder("No Program Loaded");
    return ScrollBase::Render();
}

bool DisasmView::OnEvent(Event event) { return FileViewBase::OnEvent(event); }
