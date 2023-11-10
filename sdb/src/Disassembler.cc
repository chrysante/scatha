#include "Disassembler.h"

#include <bit>
#include <sstream>

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
            str << " + " << +addr.offsetFactor << " * %" << addr.offsetRegIdx;
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

Value sdb::makeAddress(uint8_t baseRegIdx,
                       uint8_t offsetRegIdx,
                       uint8_t offsetFactor,
                       uint8_t offsetTerm) {
    return { Value::Address,
             std::bit_cast<uint32_t>(Addr{
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

std::string sdb::toString(Instruction inst) { return utl::strcat(inst); }

std::ostream& sdb::operator<<(std::ostream& str, Instruction inst) {
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
        str << " " << inst.arg1;
        break;
    case Other:
        switch (inst.opcode) {
        case OpCode::lincsp:
            str << " " << inst.arg1 << ", " << inst.arg2;
            break;
        case OpCode::call:
            str << " " << inst.arg1 << ", " << inst.arg2;
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
        case OpCode::callExt:
            break;
        default:
            assert(false);
        }
        break;
    case _count:
        assert(false);
    }
    return str;
}

template <typename T>
static T load(void const* src) {
    T t{};
    std::memcpy(&t, src, sizeof(T));
    return t;
}

Disassembly sdb::disassemble(uint8_t const* program) {
    Disassembly result;
    ProgramView const p(program);
    auto text = p.text;
    for (size_t i = 0; i < text.size();) {
        size_t binOffset = i + p.header.textOffset - sizeof(ProgramHeader);
        OpCode const opcode = static_cast<OpCode>(text[i]);
        auto const opcodeClass = classify(opcode);
        Value arg1{};
        Value arg2{};
        uint8_t const* argData = &text[i] + sizeof(OpCode);
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
                arg1 = makeValue8(load<uint8_t>(argData));
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
                break;
            case OpCode::ret:
                break;
            case OpCode::terminate:
                break;
            case OpCode::callExt:
                break;
            default:
                assert(false);
            }
            break;
        case _count:
            assert(false);
        }
        result.offsetIndexMap.insert({ binOffset, result.insts.size() });
        result.insts.push_back({ opcode, arg1, arg2 });
        i += codeSize(opcode);
    }
    return result;
}
