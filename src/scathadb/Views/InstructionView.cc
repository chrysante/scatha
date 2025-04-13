#include "Views/Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <scbinutil/OpCode.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

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

struct InstView: FileViewBase<InstView> {
    InstView(Model* model, UIHandle& uiHandle);

    void reload();
    LineInfo getLineInfo(long lineNum) const;
    ElementDecorator lineModifier(LineInfo line) const;
    Component labelRenderer(long lineNum, std::string name);
    Component instructionRenderer(long lineNum, size_t index);

    Element Render() override;
    bool OnEvent(Event event) override;

    void toggleBreakpoint(size_t index) { model->toggleInstBreakpoint(index); }
    std::optional<size_t> lineToIndex(long line) const;
    std::optional<long> indexToLine(size_t index) const;

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

InstView::InstView(Model* model, UIHandle& uiHandle): model(model) {
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

void InstView::reload() {
    DetachAllChildren();
    setFocusLine(0);
    indexToLineMap.clear();
    lineToIndexMap.clear();
    for (auto [index, inst]:
         model->disassembly().instructions() | ranges::views::enumerate)
    {
        /// Add label renderer for labelled instructions
        if (!inst.label.empty()) {
            long lineNum = static_cast<long>(ChildCount());
            Add(labelRenderer(lineNum, inst.label));
        }
        /// Add instruction renderer
        long lineNum = static_cast<long>(ChildCount());
        indexToLineMap.push_back(lineNum);
        lineToIndexMap[lineNum] = index;
        Add(instructionRenderer(lineNum, index));
    }
}

LineInfo InstView::getLineInfo(long lineNum) const {
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

ElementDecorator InstView::lineModifier(LineInfo line) const {
    using enum BreakState;
    switch (line.state) {
    case None:
        if (line.isFocused) {
            return color(Color::Black) | bgcolor(Color::GrayLight);
        }
        return nothing;
    case Step:
        return lineMessageDecorator("Step Instruction") | color(Color::White) |
               bgcolor(Color::Green);
    case Breakpoint:
        return lineMessageDecorator("Breakpoint") | color(Color::White) |
               bgcolor(Color::Green);
    case Error:
        return lineMessageDecorator(error.message()) | color(Color::White) |
               bgcolor(Color::RedLight);
    }
}

Component InstView::labelRenderer(long lineNum, std::string name) {
    return Renderer([=, this, name = std::move(name)] {
        auto line = getLineInfo(lineNum);
        return hbox({ lineNumber(line), text(std::move(name)) | bold,
                      text(":") }) |
               lineModifier(line);
    });
}

static std::string prettyInstName(scbinutil::OpCode opcode) {
    auto name = scbinutil::toString(opcode);
    using namespace std::string_view_literals;
    constexpr std::array suffixes = { "8"sv, "16"sv, "32"sv, "64"sv };
    for (auto suffix: suffixes) {
        auto pos = name.find(suffix);
        if (pos != std::string_view::npos)
            return std::string(name.substr(0, pos + suffix.size()));
    }
    return std::string(name);
}

namespace {

struct Addr {
    uint8_t baseRegIdx;
    uint8_t offsetRegIdx;
    uint8_t offsetFactor;
    uint8_t offsetTerm;
};

} // namespace

static auto labelDeco() { return bold | color(Color::GreenLight); }

static auto instDeco() {
    return size(WIDTH, GREATER_THAN, 6) | color(Color::BlueLight);
}

static auto registerDeco() { return bold | color(Color::MagentaLight); }

static auto immediateDeco() { return color(Color::CyanLight); }

static Element asRegister(uint64_t index) {
    return text(utl::strcat("%", index)) | registerDeco();
}

static Element toElement(Value value) {
    switch (value.type) {
    case Value::RegisterIndex:
        return asRegister(value.raw);
    case Value::Address: {
        auto addr = std::bit_cast<Addr>(static_cast<uint32_t>(value.raw));
        std::vector<Element> elems = { text("["), asRegister(addr.baseRegIdx) };
        if (addr.offsetRegIdx != 0xFF) {
            elems.push_back(
                text(utl::strcat(" + ", +addr.offsetFactor, " * ")));
            elems.push_back(asRegister(addr.offsetRegIdx));
        }
        if (addr.offsetTerm != 0) {
            elems.push_back(text(utl::strcat(" + ", +addr.offsetTerm)));
        }
        elems.push_back(text("]"));
        return hbox(std::move(elems));
    }
    case Value::Value8:
    case Value::Value16:
    case Value::Value32:
    case Value::Value64:
        return text(std::to_string(value.raw)) | immediateDeco();
    }
}

static Element space() { return text(" "); }

static Element comma() { return text(", "); }

static Element getLabelName(Disassembly const* disasm, Value offset) {
    if (!disasm) return text(toString(offset)) | bold;
    assert(offset.type == Value::Value32);
    auto destIndex = disasm->offsetToIndex(offset.raw);
    if (!destIndex) return text(toString(offset));
    auto& destInst = disasm->instructions()[*destIndex];
    return text(destInst.label) | labelDeco();
}

static Element printInst(Instruction inst, Disassembly const* disasm,
                         svm::VirtualMachine const* vm) {
    auto name = text(prettyInstName(inst.opcode)) | instDeco();
    using enum scbinutil::OpCodeClass;
    switch (scbinutil::classify(inst.opcode)) {
    case RR:
        [[fallthrough]];
    case RV64:
        [[fallthrough]];
    case RV32:
        [[fallthrough]];
    case RV8:
        [[fallthrough]];
    case RM:
        [[fallthrough]];
    case MR:
        return hbox({ name, space(), toElement(inst.arg1), comma(),
                      toElement(inst.arg2) });
    case R:
        return hbox({ name, space(), toElement(inst.arg1) });
    case Jump:
        return hbox({ name, space(), getLabelName(disasm, inst.arg1) });
    case Other:
        using enum scbinutil::OpCode;
        switch (inst.opcode) {
        case lincsp:
            return hbox({ name, space(), toElement(inst.arg1), comma(),
                          toElement(inst.arg2) });
        case call:
            return hbox({ name, space(), getLabelName(disasm, inst.arg1),
                          comma(), toElement(inst.arg2) });
        case icallr:
            return hbox({ name, space(), toElement(inst.arg1), comma(),
                          toElement(inst.arg2) });
        case icallm:
            return name;
        case ret:
            return name;
        case terminate:
            return name;
        case cfng:
        case cbltn: {
            auto calleeName = [&]() -> Element {
                if (vm && inst.opcode == cfng) {
                    return text(vm->getForeignFunctionName(inst.arg2.raw)) |
                           labelDeco();
                }
                else if (vm && inst.opcode == cbltn) {
                    return text(vm->getBuiltinFunctionName(inst.arg2.raw)) |
                           labelDeco();
                }
                else {
                    return text(utl::strcat("index: ", inst.arg2));
                }
            }();
            return hbox(
                { name, space(), toElement(inst.arg1), comma(), calleeName });
        }
        default:
            assert(false);
            return nullptr;
        }
    }
}

Component InstView::instructionRenderer(long lineNum, size_t index) {
    return Renderer([=, this] {
        auto line = getLineInfo(lineNum);
        auto& disasm = model->disassembly();
#if 0
        auto sourceLoc = [&] {
            size_t offset = model->disassembly().indexToOffset(index);
            if (auto SL = model->sourceDebug().sourceMap().toSourceLoc(offset))
                return text(utl::strcat("[", toString(*SL), "]"));
            return text("");
        }() | color(Color::GrayLight);
#endif
        auto instLabel =
            printInst(disasm.instruction(index), &disasm, &model->VM());
        return hbox({ lineNumber(line), breakpointIndicator(line),
                      instLabel | flex }) |
               lineModifier(line);
    });
}

Element InstView::Render() {
    if (model->disassembly().empty()) {
        return placeholder("No Program Loaded");
    }
    return ScrollBase::Render();
}

bool InstView::OnEvent(Event event) { return FileViewBase::OnEvent(event); }

std::optional<size_t> InstView::lineToIndex(long line) const {
    auto itr = lineToIndexMap.find(line);
    if (itr != lineToIndexMap.end()) {
        return itr->second;
    }
    return std::nullopt;
}

std::optional<long> InstView::indexToLine(size_t index) const {
    if (index < indexToLineMap.size()) {
        return indexToLineMap[index];
    }
    return std::nullopt;
}
