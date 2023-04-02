#include "Assembly/Map.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace Asm;

using svm::OpCode;

std::pair<OpCode, size_t> Asm::mapMove(ValueType dest,
                                       ValueType source,
                                       size_t size) {
    if (dest == ValueType::RegisterIndex) {
        switch (source) {
        case ValueType::RegisterIndex:
            return { OpCode::mov64RR, 8 };
        case ValueType::MemoryAddress:
            switch (size) {
            case 1:
                return { OpCode::mov8RM, 1 };
            case 2:
                return { OpCode::mov16RM, 2 };
            case 4:
                return { OpCode::mov32RM, 4 };
            case 8:
                return { OpCode::mov64RM, 8 };
            default:
                SC_UNREACHABLE();
            }
        case ValueType::Value8:
            [[fallthrough]];
        case ValueType::Value16:
            [[fallthrough]];
        case ValueType::Value32:
            [[fallthrough]];
        case ValueType::Value64:
            return { OpCode::mov64RV, 8 };
        default:
            SC_DEBUGFAIL(); // No matching instruction
        }
    }
    if (dest == ValueType::MemoryAddress && source == ValueType::RegisterIndex)
    {
        switch (size) {
        case 1:
            return { OpCode::mov8MR, 1 };
        case 2:
            return { OpCode::mov16MR, 2 };
        case 4:
            return { OpCode::mov32MR, 4 };
        case 8:
            return { OpCode::mov64MR, 8 };
        default:
            SC_UNREACHABLE();
        }
    }
    /// No matching instruction.
    SC_DEBUGFAIL();
}

static OpCode mapCMovRR(CompareOperation cmpOp) {
    switch (cmpOp) {
    case CompareOperation::Less:
        return OpCode::cmove64RR;
    case CompareOperation::LessEq:
        return OpCode::cmovne64RR;
    case CompareOperation::Greater:
        return OpCode::cmovl64RR;
    case CompareOperation::GreaterEq:
        return OpCode::cmovle64RR;
    case CompareOperation::Eq:
        return OpCode::cmovg64RR;
    case CompareOperation::NotEq:
        return OpCode::cmovge64RR;
    default:
        SC_UNREACHABLE();
    }
}

static OpCode mapCMovRV(CompareOperation cmpOp) {
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
    default:
        SC_UNREACHABLE();
    }
}

static OpCode mapCMovRM(CompareOperation cmpOp, size_t size) {
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
        default:
            SC_UNREACHABLE();
        }
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
        default:
            SC_UNREACHABLE();
        }
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
        default:
            SC_UNREACHABLE();
        }
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
        default:
            SC_UNREACHABLE();
        }
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
        default:
            SC_UNREACHABLE();
        }
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
        default:
            SC_UNREACHABLE();
        }
    default:
        SC_UNREACHABLE();
    }
}

std::pair<OpCode, size_t> Asm::mapCMove(CompareOperation cmpOp,
                                        ValueType dest,
                                        ValueType source,
                                        size_t size) {
    SC_ASSERT(dest == ValueType::RegisterIndex, "Can only cmov to registers");
    switch (source) {
    case ValueType::RegisterIndex:
        SC_ASSERT(size == 8, "Registers are 8 bytes");
        return { mapCMovRR(cmpOp), 8 };
    case ValueType::MemoryAddress:
        return { mapCMovRM(cmpOp, size), size };
    case ValueType::Value8:
        [[fallthrough]];
    case ValueType::Value16:
        [[fallthrough]];
    case ValueType::Value32:
        [[fallthrough]];
    case ValueType::Value64:
        return { mapCMovRV(cmpOp), 8 };
    default:
        /// No matching instruction.
        SC_DEBUGFAIL();
    }
}

OpCode Asm::mapJump(CompareOperation condition) {
    // clang-format off
    return UTL_MAP_ENUM(condition, OpCode, {
        { CompareOperation::None,      OpCode::jmp },
        { CompareOperation::Less,      OpCode::jl  },
        { CompareOperation::LessEq,    OpCode::jle },
        { CompareOperation::Greater,   OpCode::jg  },
        { CompareOperation::GreaterEq, OpCode::jge },
        { CompareOperation::Eq,        OpCode::je  },
        { CompareOperation::NotEq,     OpCode::jne },
    }); // clang-format on
}

OpCode Asm::mapCompare(Type type, ValueType lhs, ValueType rhs, size_t width) {
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::RegisterIndex) {
        switch (width) {
        case 1:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp8RR },
                { Type::Unsigned, OpCode::ucmp8RR },
                { Type::Float,    OpCode::_count },
            }); // clang-format on
        case 2:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp16RR },
                { Type::Unsigned, OpCode::ucmp16RR },
                { Type::Float,    OpCode::_count },
            }); // clang-format on
        case 4:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp32RR },
                { Type::Unsigned, OpCode::ucmp32RR },
                { Type::Float,    OpCode::fcmp32RR },
            }); // clang-format on
        case 8:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp64RR },
                { Type::Unsigned, OpCode::ucmp64RR },
                { Type::Float,    OpCode::fcmp64RR },
            }); // clang-format on
        default:
            SC_UNREACHABLE();
        }
    }
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::Value64) {
        switch (width) {
        case 1:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp8RV },
                { Type::Unsigned, OpCode::ucmp8RV },
                { Type::Float,    OpCode::_count },
            }); // clang-format on
        case 2:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp16RV },
                { Type::Unsigned, OpCode::ucmp16RV },
                { Type::Float,    OpCode::_count },
            }); // clang-format on
        case 4:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp32RV },
                { Type::Unsigned, OpCode::ucmp32RV },
                { Type::Float,    OpCode::fcmp32RV },
            }); // clang-format on
        case 8:
            // clang-format off
            return UTL_MAP_ENUM(type, OpCode, {
                { Type::Signed,   OpCode::scmp64RV },
                { Type::Unsigned, OpCode::ucmp64RV },
                { Type::Float,    OpCode::fcmp64RV },
            }); // clang-format on
        default:
            SC_UNREACHABLE();
        }
    }
    /// No matching instruction.
    SC_DEBUGFAIL();
}

OpCode Asm::mapTest(Type type, size_t width) {
    switch (width) {
    case 1:
        // clang-format off
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::stest8 },
            { Type::Unsigned, OpCode::utest8 },
            { Type::Float,    OpCode::_count },
        }); // clang-format on
    case 2:
        // clang-format off
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::stest16 },
            { Type::Unsigned, OpCode::utest16 },
            { Type::Float,    OpCode::_count },
        }); // clang-format on
    case 4:
        // clang-format off
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::stest32 },
            { Type::Unsigned, OpCode::utest32 },
            { Type::Float,    OpCode::_count },
        }); // clang-format on
    case 8:
        // clang-format off
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::stest64 },
            { Type::Unsigned, OpCode::utest64 },
            { Type::Float,    OpCode::_count },
        }); // clang-format on
    default:
        SC_UNREACHABLE();
    }
}

OpCode Asm::mapSet(CompareOperation operation) {
    // clang-format off
    return UTL_MAP_ENUM(operation, OpCode, {
        { CompareOperation::None,      OpCode::_count },
        { CompareOperation::Less,      OpCode::setl  },
        { CompareOperation::LessEq,    OpCode::setle },
        { CompareOperation::Greater,   OpCode::setg  },
        { CompareOperation::GreaterEq, OpCode::setge },
        { CompareOperation::Eq,        OpCode::sete  },
        { CompareOperation::NotEq,     OpCode::setne },
    }); // clang-format on
}

OpCode Asm::mapArithmetic(ArithmeticOperation operation,
                          ValueType dest,
                          ValueType source) {
    if (dest == ValueType::RegisterIndex && source == ValueType::RegisterIndex)
    {
        return UTL_MAP_ENUM(operation,
                            OpCode,
                            {
                                { ArithmeticOperation::Add, OpCode::addRR },
                                { ArithmeticOperation::Sub, OpCode::subRR },
                                { ArithmeticOperation::Mul, OpCode::mulRR },
                                { ArithmeticOperation::SDiv, OpCode::sdivRR },
                                { ArithmeticOperation::UDiv, OpCode::udivRR },
                                { ArithmeticOperation::SRem, OpCode::sremRR },
                                { ArithmeticOperation::URem, OpCode::uremRR },
                                { ArithmeticOperation::FAdd, OpCode::faddRR },
                                { ArithmeticOperation::FSub, OpCode::fsubRR },
                                { ArithmeticOperation::FMul, OpCode::fmulRR },
                                { ArithmeticOperation::FDiv, OpCode::fdivRR },
                                { ArithmeticOperation::LShL, OpCode::lslRR },
                                { ArithmeticOperation::LShR, OpCode::lsrRR },
                                { ArithmeticOperation::AShL, OpCode::aslRR },
                                { ArithmeticOperation::AShR, OpCode::asrRR },
                                { ArithmeticOperation::And, OpCode::andRR },
                                { ArithmeticOperation::Or, OpCode::orRR },
                                { ArithmeticOperation::XOr, OpCode::xorRR },
                            }); // clang-format on
    }
    if (dest == ValueType::RegisterIndex && source == ValueType::Value64) {
        return UTL_MAP_ENUM(operation,
                            OpCode,
                            {
                                { ArithmeticOperation::Add, OpCode::addRV },
                                { ArithmeticOperation::Sub, OpCode::subRV },
                                { ArithmeticOperation::Mul, OpCode::mulRV },
                                { ArithmeticOperation::SDiv, OpCode::sdivRV },
                                { ArithmeticOperation::UDiv, OpCode::udivRV },
                                { ArithmeticOperation::SRem, OpCode::sremRV },
                                { ArithmeticOperation::URem, OpCode::uremRV },
                                { ArithmeticOperation::FAdd, OpCode::faddRV },
                                { ArithmeticOperation::FSub, OpCode::fsubRV },
                                { ArithmeticOperation::FMul, OpCode::fmulRV },
                                { ArithmeticOperation::FDiv, OpCode::fdivRV },
                                { ArithmeticOperation::LShL, OpCode::lslRV },
                                { ArithmeticOperation::LShR, OpCode::lsrRV },
                                { ArithmeticOperation::AShL, OpCode::aslRV },
                                { ArithmeticOperation::AShR, OpCode::asrRV },
                                { ArithmeticOperation::And, OpCode::andRV },
                                { ArithmeticOperation::Or, OpCode::orRV },
                                { ArithmeticOperation::XOr, OpCode::xorRV },
                            }); // clang-format on
    }
    if (dest == ValueType::RegisterIndex && source == ValueType::MemoryAddress)
    {
        // clang-format off
        return UTL_MAP_ENUM(operation, OpCode, {
            { ArithmeticOperation::Add,  OpCode::addRM  },
            { ArithmeticOperation::Sub,  OpCode::subRM  },
            { ArithmeticOperation::Mul,  OpCode::mulRM  },
            { ArithmeticOperation::SDiv, OpCode::sdivRM },
            { ArithmeticOperation::UDiv, OpCode::udivRM },
            { ArithmeticOperation::SRem, OpCode::sremRM },
            { ArithmeticOperation::URem, OpCode::uremRM },
            { ArithmeticOperation::FAdd, OpCode::faddRM },
            { ArithmeticOperation::FSub, OpCode::fsubRM },
            { ArithmeticOperation::FMul, OpCode::fmulRM },
            { ArithmeticOperation::FDiv, OpCode::fdivRM },
            { ArithmeticOperation::LShL, OpCode::lslRM  },
            { ArithmeticOperation::LShR, OpCode::lsrRM  },
            { ArithmeticOperation::AShL, OpCode::aslRM  },
            { ArithmeticOperation::AShR, OpCode::asrRM  },
            { ArithmeticOperation::And,  OpCode::andRM  },
            { ArithmeticOperation::Or,   OpCode::orRM   },
            { ArithmeticOperation::XOr,  OpCode::xorRM  },
        }); // clang-format on
    }
    /// No matching instruction.
    SC_DEBUGFAIL();
}

// clang-format on
