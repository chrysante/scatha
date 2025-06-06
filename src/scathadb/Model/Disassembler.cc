#include "Model/Disassembler.h"

#include <bit>
#include <sstream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/VirtualMachine.h>
#include <utl/strcat.hpp>

using namespace sdb;
using namespace svm;

namespace {

struct Addr {
    uint8_t baseRegIdx;
    uint8_t offsetRegIdx;
    uint8_t offsetFactor;
    uint8_t offsetTerm;
};

struct CallExtArg {
    uint8_t regOffset;
    uint16_t idxIntoTable;
};

} // namespace

std::string sdb::toString(Value value) { return utl::strcat(value); }

std::ostream& sdb::operator<<(std::ostream& str, Value value) {
    switch (value.type) {
    case Value::RegisterIndex:
        return str << "%" << value.raw;
    case Value::Address: {
        auto addr = std::bit_cast<Addr>(static_cast<uint32_t>(value.raw));
        str << "[%" << +addr.baseRegIdx;
        if (addr.offsetRegIdx != 0xFF) {
            str << " + " << +addr.offsetFactor << " * %" << +addr.offsetRegIdx;
        }
        if (addr.offsetTerm != 0) {
            str << " + " << +addr.offsetTerm;
        }
        return str << "]";
    }
    case Value::Value8:
        [[fallthrough]];
    case Value::Value16:
        [[fallthrough]];
    case Value::Value32:
        [[fallthrough]];
    case Value::Value64:
        return str << value.raw;
    }
}

Value sdb::makeRegisterIndex(size_t index) {
    return { Value::RegisterIndex, index };
}

Value sdb::makeAddress(uint8_t baseRegIdx, uint8_t offsetRegIdx,
                       uint8_t offsetFactor, uint8_t offsetTerm) {
    return { Value::Address, std::bit_cast<uint32_t>(Addr{
                                 baseRegIdx,
                                 offsetRegIdx,
                                 offsetFactor,
                                 offsetTerm,
                             }) };
}

Value sdb::makeAddress(uint32_t value) {
    return { Value::Address, std::bit_cast<uint32_t>(value) };
}

Value sdb::makeValue8(uint64_t value) { return { Value::Value8, value }; }

Value sdb::makeValue16(uint64_t value) { return { Value::Value16, value }; }

Value sdb::makeValue32(uint64_t value) { return { Value::Value32, value }; }

Value sdb::makeValue64(uint64_t value) { return { Value::Value64, value }; }

static std::string getLabelName(Disassembly const* disasm, Value offset) {
    if (!disasm) {
        return toString(offset);
    }
    assert(offset.type == Value::Value32);
    auto destIndex = disasm->offsetToIndex(offset.raw);
    if (!destIndex) {
        return toString(offset);
    }
    auto& destInst = disasm->instructions()[*destIndex];
    return labelName(destInst.labelID);
}

static void print(std::ostream& str, Instruction inst,
                  Disassembly const* disasm, svm::VirtualMachine const* vm) {
    str << toString(inst.opcode);
    using enum OpCodeClass;
    switch (classify(inst.opcode)) {
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
        str << " " << inst.arg1 << ", " << inst.arg2;
        break;
    case R:
        str << " " << inst.arg1;
        break;
    case Jump:
        str << " " << getLabelName(disasm, inst.arg1);
        break;
    case Other:
        switch (inst.opcode) {
        case OpCode::lincsp:
            str << " " << inst.arg1 << ", " << inst.arg2;
            break;
        case OpCode::call:
            str << " " << getLabelName(disasm, inst.arg1) << ", " << inst.arg2;
            break;
        case OpCode::icallr:
            str << " " << inst.arg1 << ", " << inst.arg2;
            break;
        case OpCode::icallm:
            break;
        case OpCode::ret:
            break;
        case OpCode::terminate:
            break;
        case OpCode::cfng:
        case OpCode::cbltn:
            str << " " << /* regoffset = */ inst.arg1 << ", ";
            if (vm && inst.opcode == OpCode::cfng) {
                str << vm->getForeignFunctionName(inst.arg2.raw);
            }
            else if (vm && inst.opcode == OpCode::cbltn) {
                str << vm->getBuiltinFunctionName(inst.arg2.raw);
            }
            else {
                str << "index: " << inst.arg2;
            }
            break;
        default:
            assert(false);
        }
        break;
    }
}

std::string sdb::toString(Instruction inst, Disassembly const* disasm,
                          svm::VirtualMachine const* vm) {
    std::stringstream sstr;
    print(sstr, inst, disasm, vm);
    return std::move(sstr).str();
}

std::ostream& sdb::operator<<(std::ostream& str, Instruction inst) {
    print(str, inst, nullptr, nullptr);
    return str;
}

std::string sdb::labelName(size_t ID) {
    assert(ID != 0);
    return utl::strcat(".L", ID);
}

template <typename T>
static T load(void const* src) {
    T t{};
    std::memcpy(&t, src, sizeof(T));
    return t;
}

static Instruction readInstruction(uint8_t const* textPtr) {
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
    return { opcode, arg1, arg2 };
}

Disassembly sdb::disassemble(std::span<uint8_t const> program) {
    if (program.empty()) {
        return {};
    }

    Disassembly result;
    ProgramView const p(program.data());
    auto text = p.text;

    /// Gather all instructions
    for (size_t i = 0; i < text.size();) {
        size_t binOffset = i + p.header.textOffset - sizeof(ProgramHeader);
        auto inst = readInstruction(&text[i]);
        result.offsetToIndexMap.insert({ binOffset, result.insts.size() });
        result.indexToOffsetMap.push_back(binOffset);
        result.insts.push_back(inst);
        i += codeSize(inst.opcode);
    }

    /// Gather indices of all labelled instructions, i.e. instructions that are
    /// targets of jumps or calls
    auto labelledInstructionIndices = result.insts | ranges::views::enumerate |
                                      ranges::views::filter([](auto p) {
        auto [index, inst] = p;
        return inst.opcode == OpCode::call ||
               classify(inst.opcode) == OpCodeClass::Jump;
    }) | ranges::views::values | ranges::views::transform([&](auto& inst) {
        return result.offsetToIndex(inst.arg1.raw).value();
    }) | ranges::to<std::vector>;

    ranges::sort(labelledInstructionIndices);
    ranges::unique(labelledInstructionIndices);

    /// Assign ascending labels to all labelled instructions
    size_t labelID = 1;
    ranges::for_each(labelledInstructionIndices, [&](size_t index) {
        auto& destInst = result.insts[index];
        destInst.labelID = labelID++;
    });

    return result;
}
