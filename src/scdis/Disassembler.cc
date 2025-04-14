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

Disassembly Disassembler::run() {
    if (program.empty()) return {};
    Disassembly result;
    ProgramView p(program.data());
    auto text = p.text;
    // Gather all instructions and labels from debug info
    for (size_t textIndex = 0; textIndex < text.size();) {
        size_t instIndex = result._insts.size();
        InstructionPointerOffset ipo(textIndex + p.header.textOffset -
                                     sizeof(ProgramHeader));
        result._indexMap.insertAtBack(ipo);
        auto labelItr = debugInfo.labelMap.find(ipo.value);
        if (labelItr != debugInfo.labelMap.end())
            result._labels.insert({ instIndex, Label{ labelItr->second } });
        auto inst = readInstruction(ipo, &text[textIndex]);
        result._insts.push_back(inst);
        textIndex += scbinutil::codeSize(inst.opcode);
    }
    // Try to reconstruct labels by identifying targets of jump and call
    // instructions. If we have complete debug info this should be a no-op.
    auto jumpTargetIndices = result._insts | filter([](auto& inst) {
        return inst.opcode == OpCode::call ||
               classify(inst.opcode) == OpCodeClass::Jump;
    }) | transform([&](auto& inst) {
        return result.indexMap()
            .ipoToIndex(InstructionPointerOffset(inst.arg1.raw))
            .value();
    }) | ranges::to<std::vector>;
    ranges::sort(jumpTargetIndices);
    ranges::unique(jumpTargetIndices);
    // Assign ascending labels to all labelled instructions
    size_t labelID = 1;
    ranges::for_each(jumpTargetIndices, [&](size_t index) {
        result._labels.try_emplace(index, utl::lazy([&] {
            return Label{ utl::strcat(".L", labelID++) };
        }));
    });
    // Gather all FFIs
    for (auto& lib: p.libDecls)
        for (auto& ffi: lib.funcDecls)
            result._ffiset.insert(std::move(ffi));
    return result;
}
