#include "Assembly/Map.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace Asm;

using scbinutil::OpCode;

std::optional<MoveMapResult> Asm::mapMove(ValueType dest, ValueType source,
                                          size_t size) {
    if (dest == ValueType::RegisterIndex) {
        switch (source) {
        case ValueType::RegisterIndex:
            return MoveMapResult{ OpCode::mov64RR, 8 };
        case ValueType::MemoryAddress:
            switch (size) {
            case 1:
                return MoveMapResult{ OpCode::mov8RM, 1 };
            case 2:
                return MoveMapResult{ OpCode::mov16RM, 2 };
            case 4:
                return MoveMapResult{ OpCode::mov32RM, 4 };
            case 8:
                return MoveMapResult{ OpCode::mov64RM, 8 };
            default:
                return std::nullopt;
            }
        case ValueType::Value8:
            [[fallthrough]];
        case ValueType::Value16:
            [[fallthrough]];
        case ValueType::Value32:
            [[fallthrough]];
        case ValueType::Value64:
            [[fallthrough]];
        case ValueType::LabelPosition:
            return MoveMapResult{ OpCode::mov64RV, 8 };
        default:
            /// No matching instruction
            return std::nullopt;
        }
    }
    if (dest == ValueType::MemoryAddress && source == ValueType::RegisterIndex)
    {
        switch (size) {
        case 1:
            return MoveMapResult{ OpCode::mov8MR, 1 };
        case 2:
            return MoveMapResult{ OpCode::mov16MR, 2 };
        case 4:
            return MoveMapResult{ OpCode::mov32MR, 4 };
        case 8:
            return MoveMapResult{ OpCode::mov64MR, 8 };
        default:
            return std::nullopt;
        }
    }
    /// No matching instruction.
    return std::nullopt;
}

static std::optional<OpCode> mapCMovRR(CompareOperation cmpOp) {
    switch (cmpOp) {
    case CompareOperation::Less:
        return OpCode::cmovl64RR;
    case CompareOperation::LessEq:
        return OpCode::cmovle64RR;
    case CompareOperation::Greater:
        return OpCode::cmovg64RR;
    case CompareOperation::GreaterEq:
        return OpCode::cmovge64RR;
    case CompareOperation::Eq:
        return OpCode::cmove64RR;
    case CompareOperation::NotEq:
        return OpCode::cmovne64RR;
    case CompareOperation::None:
        return std::nullopt;
    }
    return std::nullopt;
}

static std::optional<OpCode> mapCMovRV(CompareOperation cmpOp) {
    switch (cmpOp) {
    case CompareOperation::Less:
        return OpCode::cmovl64RV;
    case CompareOperation::LessEq:
        return OpCode::cmovle64RV;
    case CompareOperation::Greater:
        return OpCode::cmovg64RV;
    case CompareOperation::GreaterEq:
        return OpCode::cmovge64RV;
    case CompareOperation::Eq:
        return OpCode::cmove64RV;
    case CompareOperation::NotEq:
        return OpCode::cmovne64RV;
    case CompareOperation::None:
        return std::nullopt;
    }
    return std::nullopt;
}

static std::optional<OpCode> mapCMovRM(CompareOperation cmpOp, size_t size) {
    switch (cmpOp) {
    case CompareOperation::Less:
        switch (size) {
        case 1:
            return OpCode::cmovl8RM;
        case 2:
            return OpCode::cmovl16RM;
        case 4:
            return OpCode::cmovl32RM;
        case 8:
            return OpCode::cmovl64RM;
        }
        return std::nullopt;
    case CompareOperation::LessEq:
        switch (size) {
        case 1:
            return OpCode::cmovle8RM;
        case 2:
            return OpCode::cmovle16RM;
        case 4:
            return OpCode::cmovle32RM;
        case 8:
            return OpCode::cmovle64RM;
        }
        return std::nullopt;
    case CompareOperation::Greater:
        switch (size) {
        case 1:
            return OpCode::cmovg8RM;
        case 2:
            return OpCode::cmovg16RM;
        case 4:
            return OpCode::cmovg32RM;
        case 8:
            return OpCode::cmovg64RM;
        }
        return std::nullopt;
    case CompareOperation::GreaterEq:
        switch (size) {
        case 1:
            return OpCode::cmovge8RM;
        case 2:
            return OpCode::cmovge16RM;
        case 4:
            return OpCode::cmovge32RM;
        case 8:
            return OpCode::cmovge64RM;
        }
        return std::nullopt;
    case CompareOperation::Eq:
        switch (size) {
        case 1:
            return OpCode::cmove8RM;
        case 2:
            return OpCode::cmove16RM;
        case 4:
            return OpCode::cmove32RM;
        case 8:
            return OpCode::cmove64RM;
        }
        return std::nullopt;
    case CompareOperation::NotEq:
        switch (size) {
        case 1:
            return OpCode::cmovne8RM;
        case 2:
            return OpCode::cmovne16RM;
        case 4:
            return OpCode::cmovne32RM;
        case 8:
            return OpCode::cmovne64RM;
        }
        return std::nullopt;
    case CompareOperation::None:
        return std::nullopt;
    }
    SC_UNREACHABLE();
}

std::optional<MoveMapResult> Asm::mapCMove(CompareOperation cmpOp,
                                           ValueType dest, ValueType source,
                                           size_t size) {
    SC_ASSERT(dest == ValueType::RegisterIndex, "Can only cmov to registers");
    switch (source) {
    case ValueType::RegisterIndex:
        if (auto opcode = mapCMovRR(cmpOp)) {
            return MoveMapResult{ *opcode, 8 };
        }
        return std::nullopt;
    case ValueType::MemoryAddress:
        if (auto opcode = mapCMovRM(cmpOp, size)) {
            return MoveMapResult{ *opcode, size };
        }
        return std::nullopt;
    case ValueType::Value8:
        [[fallthrough]];
    case ValueType::Value16:
        [[fallthrough]];
    case ValueType::Value32:
        [[fallthrough]];
    case ValueType::Value64:
        if (auto opcode = mapCMovRV(cmpOp)) {
            return MoveMapResult{ *opcode, 8 };
        }
        return std::nullopt;
    case ValueType::LabelPosition:
        return std::nullopt;
    }
    SC_UNREACHABLE();
}

std::optional<OpCode> Asm::mapJump(CompareOperation condition) {
    switch (condition) {
    case CompareOperation::None:
        return OpCode::jmp;
    case CompareOperation::Less:
        return OpCode::jl;
    case CompareOperation::LessEq:
        return OpCode::jle;
    case CompareOperation::Greater:
        return OpCode::jg;
    case CompareOperation::GreaterEq:
        return OpCode::jge;
    case CompareOperation::Eq:
        return OpCode::je;
    case CompareOperation::NotEq:
        return OpCode::jne;
    }
    SC_UNREACHABLE();
}

std::optional<OpCode> Asm::mapCall(ValueType type) {
    switch (type) {
    case ValueType::LabelPosition:
        return OpCode::call;
    case ValueType::RegisterIndex:
        return OpCode::icallr;
    case ValueType::MemoryAddress:
        return OpCode::icallm;
    default:
        return std::nullopt;
    }
}

std::optional<OpCode> Asm::mapCompare(Type type, ValueType lhs, ValueType rhs,
                                      size_t width) {
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::RegisterIndex) {
        switch (width) {
        case 1:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp8RR;
            case Type::Unsigned:
                return OpCode::ucmp8RR;
            case Type::Float:
                return std::nullopt;
            }
            return std::nullopt;
        case 2:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp16RR;
            case Type::Unsigned:
                return OpCode::ucmp16RR;
            case Type::Float:
                return std::nullopt;
            }
            return std::nullopt;
        case 4:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp32RR;
            case Type::Unsigned:
                return OpCode::ucmp32RR;
            case Type::Float:
                return OpCode::fcmp32RR;
            }
            return std::nullopt;
        case 8:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp64RR;
            case Type::Unsigned:
                return OpCode::ucmp64RR;
            case Type::Float:
                return OpCode::fcmp64RR;
            }
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }
    if (lhs == ValueType::RegisterIndex &&
        (rhs == ValueType::Value64 || rhs == ValueType::LabelPosition))
    {
        switch (width) {
        case 1:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp8RV;
            case Type::Unsigned:
                return OpCode::ucmp8RV;
            case Type::Float:
                return std::nullopt;
            }
            return std::nullopt;
        case 2:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp16RV;
            case Type::Unsigned:
                return OpCode::ucmp16RV;
            case Type::Float:
                return std::nullopt;
            }
            return std::nullopt;
        case 4:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp32RV;
            case Type::Unsigned:
                return OpCode::ucmp32RV;
            case Type::Float:
                return OpCode::fcmp32RV;
            }
            return std::nullopt;
        case 8:
            switch (type) {
            case Type::Signed:
                return OpCode::scmp64RV;
            case Type::Unsigned:
                return OpCode::ucmp64RV;
            case Type::Float:
                return OpCode::fcmp64RV;
            }
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }
    /// No matching instruction.
    return std::nullopt;
}

std::optional<OpCode> Asm::mapTest(Type type, size_t width) {
    switch (width) {
    case 1:
        switch (type) {
        case Type::Signed:
            return OpCode::stest8;
        case Type::Unsigned:
            return OpCode::utest8;
        case Type::Float:
            return std::nullopt;
        }
    case 2:
        switch (type) {
        case Type::Signed:
            return OpCode::stest16;
        case Type::Unsigned:
            return OpCode::utest16;
        case Type::Float:
            return std::nullopt;
        }
    case 4:
        switch (type) {
        case Type::Signed:
            return OpCode::stest32;
        case Type::Unsigned:
            return OpCode::utest32;
        case Type::Float:
            return std::nullopt;
        }
    case 8:
        switch (type) {
        case Type::Signed:
            return OpCode::stest64;
        case Type::Unsigned:
            return OpCode::utest64;
        case Type::Float:
            return std::nullopt;
        }
    default:
        return std::nullopt;
    }
}

std::optional<OpCode> Asm::mapSet(CompareOperation operation) {
    switch (operation) {
    case CompareOperation::None:
        return std::nullopt;
    case CompareOperation::Less:
        return OpCode::setl;
    case CompareOperation::LessEq:
        return OpCode::setle;
    case CompareOperation::Greater:
        return OpCode::setg;
    case CompareOperation::GreaterEq:
        return OpCode::setge;
    case CompareOperation::Eq:
        return OpCode::sete;
    case CompareOperation::NotEq:
        return OpCode::setne;
    }
    SC_UNREACHABLE();
}

std::optional<OpCode> Asm::mapArithmetic64(ArithmeticOperation operation,
                                           ValueType dest, ValueType source) {
    if (dest == ValueType::RegisterIndex && source == ValueType::RegisterIndex)
    {
        switch (operation) {
        case ArithmeticOperation::Add:
            return OpCode::add64RR;
        case ArithmeticOperation::Sub:
            return OpCode::sub64RR;
        case ArithmeticOperation::Mul:
            return OpCode::mul64RR;
        case ArithmeticOperation::SDiv:
            return OpCode::sdiv64RR;
        case ArithmeticOperation::UDiv:
            return OpCode::udiv64RR;
        case ArithmeticOperation::SRem:
            return OpCode::srem64RR;
        case ArithmeticOperation::URem:
            return OpCode::urem64RR;
        case ArithmeticOperation::FAdd:
            return OpCode::fadd64RR;
        case ArithmeticOperation::FSub:
            return OpCode::fsub64RR;
        case ArithmeticOperation::FMul:
            return OpCode::fmul64RR;
        case ArithmeticOperation::FDiv:
            return OpCode::fdiv64RR;
        case ArithmeticOperation::LShL:
            return OpCode::lsl64RR;
        case ArithmeticOperation::LShR:
            return OpCode::lsr64RR;
        case ArithmeticOperation::AShL:
            return OpCode::asl64RR;
        case ArithmeticOperation::AShR:
            return OpCode::asr64RR;
        case ArithmeticOperation::And:
            return OpCode::and64RR;
        case ArithmeticOperation::Or:
            return OpCode::or64RR;
        case ArithmeticOperation::XOr:
            return OpCode::xor64RR;
        }
        SC_UNREACHABLE();
    }
    if (dest == ValueType::RegisterIndex &&
        (source == ValueType::Value64 || source == ValueType::Value8))
    {
        SC_ASSERT((source == ValueType::Value8) == isShift(operation),
                  "Only shift operations allow 8 bit literal operands");
        switch (operation) {
        case ArithmeticOperation::Add:
            return OpCode::add64RV;
        case ArithmeticOperation::Sub:
            return OpCode::sub64RV;
        case ArithmeticOperation::Mul:
            return OpCode::mul64RV;
        case ArithmeticOperation::SDiv:
            return OpCode::sdiv64RV;
        case ArithmeticOperation::UDiv:
            return OpCode::udiv64RV;
        case ArithmeticOperation::SRem:
            return OpCode::srem64RV;
        case ArithmeticOperation::URem:
            return OpCode::urem64RV;
        case ArithmeticOperation::FAdd:
            return OpCode::fadd64RV;
        case ArithmeticOperation::FSub:
            return OpCode::fsub64RV;
        case ArithmeticOperation::FMul:
            return OpCode::fmul64RV;
        case ArithmeticOperation::FDiv:
            return OpCode::fdiv64RV;
        case ArithmeticOperation::LShL:
            return OpCode::lsl64RV;
        case ArithmeticOperation::LShR:
            return OpCode::lsr64RV;
        case ArithmeticOperation::AShL:
            return OpCode::asl64RV;
        case ArithmeticOperation::AShR:
            return OpCode::asr64RV;
        case ArithmeticOperation::And:
            return OpCode::and64RV;
        case ArithmeticOperation::Or:
            return OpCode::or64RV;
        case ArithmeticOperation::XOr:
            return OpCode::xor64RV;
        }
        SC_UNREACHABLE();
    }
    if (dest == ValueType::RegisterIndex && source == ValueType::MemoryAddress)
    {
        switch (operation) {
        case ArithmeticOperation::Add:
            return OpCode::add64RM;
        case ArithmeticOperation::Sub:
            return OpCode::sub64RM;
        case ArithmeticOperation::Mul:
            return OpCode::mul64RM;
        case ArithmeticOperation::SDiv:
            return OpCode::sdiv64RM;
        case ArithmeticOperation::UDiv:
            return OpCode::udiv64RM;
        case ArithmeticOperation::SRem:
            return OpCode::srem64RM;
        case ArithmeticOperation::URem:
            return OpCode::urem64RM;
        case ArithmeticOperation::FAdd:
            return OpCode::fadd64RM;
        case ArithmeticOperation::FSub:
            return OpCode::fsub64RM;
        case ArithmeticOperation::FMul:
            return OpCode::fmul64RM;
        case ArithmeticOperation::FDiv:
            return OpCode::fdiv64RM;
        case ArithmeticOperation::LShL:
            return OpCode::lsl64RM;
        case ArithmeticOperation::LShR:
            return OpCode::lsr64RM;
        case ArithmeticOperation::AShL:
            return OpCode::asl64RM;
        case ArithmeticOperation::AShR:
            return OpCode::asr64RM;
        case ArithmeticOperation::And:
            return OpCode::and64RM;
        case ArithmeticOperation::Or:
            return OpCode::or64RM;
        case ArithmeticOperation::XOr:
            return OpCode::xor64RM;
        }
        SC_UNREACHABLE();
    }
    /// No matching instruction.
    return std::nullopt;
}

std::optional<OpCode> Asm::mapArithmetic32(ArithmeticOperation operation,
                                           ValueType dest, ValueType source) {
    if (dest == ValueType::RegisterIndex && source == ValueType::RegisterIndex)
    {
        switch (operation) {
        case ArithmeticOperation::Add:
            return OpCode::add32RR;
        case ArithmeticOperation::Sub:
            return OpCode::sub32RR;
        case ArithmeticOperation::Mul:
            return OpCode::mul32RR;
        case ArithmeticOperation::SDiv:
            return OpCode::sdiv32RR;
        case ArithmeticOperation::UDiv:
            return OpCode::udiv32RR;
        case ArithmeticOperation::SRem:
            return OpCode::srem32RR;
        case ArithmeticOperation::URem:
            return OpCode::urem32RR;
        case ArithmeticOperation::FAdd:
            return OpCode::fadd32RR;
        case ArithmeticOperation::FSub:
            return OpCode::fsub32RR;
        case ArithmeticOperation::FMul:
            return OpCode::fmul32RR;
        case ArithmeticOperation::FDiv:
            return OpCode::fdiv32RR;
        case ArithmeticOperation::LShL:
            return OpCode::lsl32RR;
        case ArithmeticOperation::LShR:
            return OpCode::lsr32RR;
        case ArithmeticOperation::AShL:
            return OpCode::asl32RR;
        case ArithmeticOperation::AShR:
            return OpCode::asr32RR;
        case ArithmeticOperation::And:
            return OpCode::and32RR;
        case ArithmeticOperation::Or:
            return OpCode::or32RR;
        case ArithmeticOperation::XOr:
            return OpCode::xor32RR;
        }
        SC_UNREACHABLE();
    }
    if (dest == ValueType::RegisterIndex &&
        (source == ValueType::Value32 || source == ValueType::Value8))
    {
        SC_ASSERT((source == ValueType::Value8) == isShift(operation),
                  "Only shift operations allow 8 bit literal operands");
        switch (operation) {
        case ArithmeticOperation::Add:
            return OpCode::add32RV;
        case ArithmeticOperation::Sub:
            return OpCode::sub32RV;
        case ArithmeticOperation::Mul:
            return OpCode::mul32RV;
        case ArithmeticOperation::SDiv:
            return OpCode::sdiv32RV;
        case ArithmeticOperation::UDiv:
            return OpCode::udiv32RV;
        case ArithmeticOperation::SRem:
            return OpCode::srem32RV;
        case ArithmeticOperation::URem:
            return OpCode::urem32RV;
        case ArithmeticOperation::FAdd:
            return OpCode::fadd32RV;
        case ArithmeticOperation::FSub:
            return OpCode::fsub32RV;
        case ArithmeticOperation::FMul:
            return OpCode::fmul32RV;
        case ArithmeticOperation::FDiv:
            return OpCode::fdiv32RV;
        case ArithmeticOperation::LShL:
            return OpCode::lsl32RV;
        case ArithmeticOperation::LShR:
            return OpCode::lsr32RV;
        case ArithmeticOperation::AShL:
            return OpCode::asl32RV;
        case ArithmeticOperation::AShR:
            return OpCode::asr32RV;
        case ArithmeticOperation::And:
            return OpCode::and32RV;
        case ArithmeticOperation::Or:
            return OpCode::or32RV;
        case ArithmeticOperation::XOr:
            return OpCode::xor32RV;
        }
        SC_UNREACHABLE();
    }
    if (dest == ValueType::RegisterIndex && source == ValueType::MemoryAddress)
    {
        switch (operation) {
        case ArithmeticOperation::Add:
            return OpCode::add32RM;
        case ArithmeticOperation::Sub:
            return OpCode::sub32RM;
        case ArithmeticOperation::Mul:
            return OpCode::mul32RM;
        case ArithmeticOperation::SDiv:
            return OpCode::sdiv32RM;
        case ArithmeticOperation::UDiv:
            return OpCode::udiv32RM;
        case ArithmeticOperation::SRem:
            return OpCode::srem32RM;
        case ArithmeticOperation::URem:
            return OpCode::urem32RM;
        case ArithmeticOperation::FAdd:
            return OpCode::fadd32RM;
        case ArithmeticOperation::FSub:
            return OpCode::fsub32RM;
        case ArithmeticOperation::FMul:
            return OpCode::fmul32RM;
        case ArithmeticOperation::FDiv:
            return OpCode::fdiv32RM;
        case ArithmeticOperation::LShL:
            return OpCode::lsl32RM;
        case ArithmeticOperation::LShR:
            return OpCode::lsr32RM;
        case ArithmeticOperation::AShL:
            return OpCode::asl32RM;
        case ArithmeticOperation::AShR:
            return OpCode::asr32RM;
        case ArithmeticOperation::And:
            return OpCode::and32RM;
        case ArithmeticOperation::Or:
            return OpCode::or32RM;
        case ArithmeticOperation::XOr:
            return OpCode::xor32RM;
        }
        SC_UNREACHABLE();
    }
    /// No matching instruction.
    return std::nullopt;
}

// clang-format on
