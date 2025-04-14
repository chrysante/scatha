#include "scdis/Disassembler.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <scbinutil/ProgramView.h>
#include <utl/lazy.hpp>
#include <utl/strcat.hpp>

using namespace scdis;
using namespace scbinutil;
using namespace ranges::views;

using scdis::internal::Disassembler;

struct scdis::internal::Disassembler {
    std::span<uint8_t const> program;
    scatha::DebugInfoMap const& debugInfo;

    Disassembly run();
};

Disassembly scdis::disassemble(std::span<uint8_t const> program,
                               scatha::DebugInfoMap const& debugInfo) {
    return Disassembler{ program, debugInfo }.run();
}

template <typename T>
static T load(void const* src) {
    T t{};
    std::memcpy(&t, src, sizeof(T));
    return t;
}

static Instruction readInstruction(InstructionPointerOffset ipo,
                                   uint8_t const* textPtr) {
    OpCode const opcode = static_cast<OpCode>(*textPtr);
    auto const opcodeClass = classify(opcode);
    Value arg1{};
    Value arg2{};
    uint8_t const* argData = textPtr + sizeof(OpCode);
    switch (opcodeClass) {
        using enum OpCodeClass;
    case RR:
        arg1 = makeRegisterIndex(load<uint8_t>(argData));
        arg2 = makeRegisterIndex(load<uint8_t>(argData + 1));
        break;
    case RV64:
        arg1 = makeRegisterIndex(load<uint8_t>(argData));
        arg2 = makeValue64(load<uint64_t>(argData + 1));
        break;
    case RV32:
        arg1 = makeRegisterIndex(load<uint8_t>(argData));
        arg2 = makeValue32(load<uint32_t>(argData + 1));
        break;
    case RV8:
        arg1 = makeRegisterIndex(load<uint8_t>(argData));
        arg2 = makeValue8(load<uint8_t>(argData + 1));
        break;
    case RM:
        arg1 = makeRegisterIndex(load<uint8_t>(argData));
        arg2 = makeAddress(load<uint32_t>(argData + 1));
        break;
    case MR:
        arg1 = makeAddress(load<uint32_t>(argData));
        arg2 = makeRegisterIndex(load<uint8_t>(argData + 4));
        break;
    case R:
        arg1 = makeRegisterIndex(load<uint8_t>(argData));
        break;
    case Jump:
        arg1 = makeValue32(load<uint32_t>(argData));
        break;
    case Other:
        switch (opcode) {
        case OpCode::lincsp:
            arg1 = makeRegisterIndex(load<uint8_t>(argData));
            arg2 = makeValue16(load<uint16_t>(argData + 1));
            break;
        case OpCode::call:
            arg1 = makeValue32(load<uint32_t>(argData));
            arg2 = makeValue8(load<uint8_t>(argData + 4));
            break;
        case OpCode::icallr:
            arg1 = makeRegisterIndex(load<uint8_t>(argData));
            arg2 = makeValue8(load<uint8_t>(argData + 1));
            break;
        case OpCode::icallm:
            arg1 = makeAddress(load<uint32_t>(argData));
            arg2 = makeValue8(load<uint32_t>(argData + 4));
            break;
        case OpCode::ret:
            break;
        case OpCode::terminate:
            break;
        case OpCode::cfng:
        case OpCode::cbltn:
            arg1 = makeValue8(load<uint8_t>(argData));
            arg2 = makeValue16(load<uint8_t>(argData + 1));
            break;
        default:
            assert(false);
        }
        break;
    default:
        assert(false);
    }
    return { ipo, opcode, arg1, arg2 };
}

static Label::Type convertType(scatha::DebugLabel::Type type) {
    switch (type) {
    case scatha::DebugLabel::Function:
        return Label::Function;
    case scatha::DebugLabel::BasicBlock:
        return Label::Block;
    case scatha::DebugLabel::StringData:
        return Label::String;
    case scatha::DebugLabel::RawData:
        return Label::Raw;
    }
}

static Label convertLabel(scatha::DebugLabel const& label) {
    return { convertType(label.type), label.name };
}

Disassembly Disassembler::run() {
    if (program.empty()) return {};
    Disassembly result;
    ProgramView p(program.data());
    // Gather all global variables
    auto data = p.data;
    std::vector<std::pair<InstructionPointerOffset, scatha::DebugLabel>>
        dataLabels;
    for (auto& [binOffset, debugLabel]: debugInfo.labelMap)
        if ((int)debugLabel.type >= (int)scatha::DebugLabel::StringData)
            dataLabels.emplace_back(InstructionPointerOffset(binOffset),
                                    debugLabel);
    ranges::sort(dataLabels, [](auto& a, auto& b) {
        return a.first.value < b.first.value;
    });
    for (size_t i = 0; i < dataLabels.size(); ++i) {
        auto& [ipoBegin, label] = dataLabels[i];
        auto ipoEnd = i < dataLabels.size() - 1 ?
                          dataLabels[i + 1].first :
                          InstructionPointerOffset(data.size());
        result._vars.push_back({ .label = convertLabel(label),
                                 .ipo = ipoBegin,
                                 .data{ data.data() + ipoBegin.value,
                                        data.data() + ipoEnd.value } });
    }
    // If we don't have debug info we treat the entire data section as one
    // variable
    if (result._vars.empty() && !data.empty())
        result._vars.push_back(
            { .label = { .type = Label::Raw, .name = ".data" },
              .ipo{ 0 },
              .data{ data.begin(), data.end() } });
    // Gather all instructions and labels from debug info
    auto text = p.text;
    for (size_t textIndex = 0; textIndex < text.size();) {
        size_t instIndex = result._insts.size();
        InstructionPointerOffset ipo(textIndex + p.header.textOffset -
                                     sizeof(ProgramHeader));
        result._indexMap.insertAtBack(ipo);
        auto labelItr = debugInfo.labelMap.find(ipo.value);
        if (labelItr != debugInfo.labelMap.end())
            result._instLabels.insert(
                { instIndex, convertLabel(labelItr->second) });
        auto inst = readInstruction(ipo, &text[textIndex]);
        result._insts.push_back(inst);
        textIndex += scbinutil::codeSize(inst.opcode);
    }
    // Try to reconstruct labels by identifying targets of jump and call
    // instructions. If we have complete debug info this should be a no-op.
    struct DerivedLabel {
        Label::Type type;
        size_t instIndex;
    };
    auto jumpTargetIndices = result._insts | filter([](auto& inst) {
        return inst.opcode == OpCode::call ||
               classify(inst.opcode) == OpCodeClass::Jump;
    }) | transform([&](auto& inst) {
        return DerivedLabel{
            inst.opcode == OpCode::call ? Label::Function : Label::Block,
            result.indexMap()
                .ipoToIndex(InstructionPointerOffset(inst.arg1.raw))
                .value()
        };
    }) | ranges::to<std::vector>;
    auto less = [](DerivedLabel const& a, DerivedLabel const& b) {
        return std::tuple(a.instIndex, (int)a.type) <
               std::tuple(b.instIndex, (int)b.type);
    };
    ranges::sort(jumpTargetIndices, less);
    ranges::unique(jumpTargetIndices, less);
    // Assign ascending labels to all labelled instructions
    size_t blockID = 1, funcID = 1;
    ranges::for_each(jumpTargetIndices, [&](DerivedLabel label) {
        result._instLabels.try_emplace(label.instIndex, utl::lazy([&] {
            auto name = label.type == Label::Function ?
                            utl::strcat(".F", funcID++) :
                            utl::strcat(".L", blockID++);
            return Label{ .type = label.type, .name = std::move(name) };
        }));
    });
    result._instLabels.try_emplace(0, utl::lazy([&] {
        return Label{ .type = Label::Block, .name = ".text" };
    }));
    // Gather all FFIs
    for (auto& lib: p.libDecls)
        for (auto& ffi: lib.funcDecls)
            result._ffiset.insert(std::move(ffi));
    return result;
}
