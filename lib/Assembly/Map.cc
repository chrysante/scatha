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

OpCode Asm::mapCompare(Type type, ValueType lhs, ValueType rhs) {
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::RegisterIndex) {
        // clang-format off
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::icmpRR },
            { Type::Unsigned, OpCode::ucmpRR },
            { Type::Float,    OpCode::fcmpRR },
        }); // clang-format on
    }
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::Value64) {
        // clang-format off
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::icmpRV },
            { Type::Unsigned, OpCode::ucmpRV },
            { Type::Float,    OpCode::fcmpRV },
        }); // clang-format on
    }
    /// No matching instruction.
    SC_DEBUGFAIL();
}

OpCode Asm::mapTest(Type type) {
    // clang-format off
    return UTL_MAP_ENUM(type, OpCode, {
        { Type::Signed,   OpCode::itest  },
        { Type::Unsigned, OpCode::utest  },
        { Type::Float,    OpCode::_count },
    }); // clang-format on
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
                          Type type,
                          ValueType dest,
                          ValueType source) {
    if (dest == ValueType::RegisterIndex && source == ValueType::RegisterIndex)
    {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRR  },
                { ArithmeticOperation::Sub,  OpCode::subRR  },
                { ArithmeticOperation::Mul,  OpCode::mulRR  },
                { ArithmeticOperation::Div,  OpCode::idivRR },
                { ArithmeticOperation::Rem,  OpCode::iremRR },
                { ArithmeticOperation::LShL, OpCode::lslRR  },
                { ArithmeticOperation::LShR, OpCode::lsrRR  },
                { ArithmeticOperation::AShL, OpCode::aslRR  },
                { ArithmeticOperation::AShR, OpCode::asrRR  },
                { ArithmeticOperation::And,  OpCode::andRR  },
                { ArithmeticOperation::Or,   OpCode::orRR   },
                { ArithmeticOperation::XOr,  OpCode::xorRR  },
            }); // clang-format on
        case Type::Unsigned:
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRR  },
                { ArithmeticOperation::Sub,  OpCode::subRR  },
                { ArithmeticOperation::Mul,  OpCode::mulRR  },
                { ArithmeticOperation::Div,  OpCode::divRR  },
                { ArithmeticOperation::Rem,  OpCode::remRR  },
                { ArithmeticOperation::LShL, OpCode::lslRR  },
                { ArithmeticOperation::LShR, OpCode::lsrRR  },
                { ArithmeticOperation::AShL, OpCode::aslRR  },
                { ArithmeticOperation::AShR, OpCode::asrRR  },
                { ArithmeticOperation::And,  OpCode::andRR  },
                { ArithmeticOperation::Or,   OpCode::orRR   },
                { ArithmeticOperation::XOr,  OpCode::xorRR  },
            }); // clang-format on
        case Type::Float:
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::faddRR },
                { ArithmeticOperation::Sub,  OpCode::fsubRR },
                { ArithmeticOperation::Mul,  OpCode::fmulRR },
                { ArithmeticOperation::Div,  OpCode::fdivRR },
                { ArithmeticOperation::Rem,  OpCode::_count },
                { ArithmeticOperation::LShL,  OpCode::_count },
                { ArithmeticOperation::LShR,  OpCode::_count },
                { ArithmeticOperation::AShL,  OpCode::_count },
                { ArithmeticOperation::AShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            }); // clang-format on
        case Type::_count:
            SC_UNREACHABLE();
        }
    }
    if (dest == ValueType::RegisterIndex && source == ValueType::Value64) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRV  },
                { ArithmeticOperation::Sub,  OpCode::subRV  },
                { ArithmeticOperation::Mul,  OpCode::mulRV  },
                { ArithmeticOperation::Div,  OpCode::idivRV },
                { ArithmeticOperation::Rem,  OpCode::iremRV },
                { ArithmeticOperation::LShL, OpCode::lslRV  },
                { ArithmeticOperation::LShR, OpCode::lsrRV  },
                { ArithmeticOperation::AShL, OpCode::aslRV  },
                { ArithmeticOperation::AShR, OpCode::asrRV  },
                { ArithmeticOperation::And,  OpCode::andRV  },
                { ArithmeticOperation::Or,   OpCode::orRV   },
                { ArithmeticOperation::XOr,  OpCode::xorRV  },
            }); // clang-format on
        case Type::Unsigned:
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRV  },
                { ArithmeticOperation::Sub,  OpCode::subRV  },
                { ArithmeticOperation::Mul,  OpCode::mulRV  },
                { ArithmeticOperation::Div,  OpCode::divRV  },
                { ArithmeticOperation::Rem,  OpCode::remRV  },
                { ArithmeticOperation::LShL, OpCode::lslRV  },
                { ArithmeticOperation::LShR, OpCode::lsrRV  },
                { ArithmeticOperation::AShL, OpCode::aslRV  },
                { ArithmeticOperation::AShR, OpCode::asrRV  },
                { ArithmeticOperation::And,  OpCode::andRV  },
                { ArithmeticOperation::Or,   OpCode::orRV   },
                { ArithmeticOperation::XOr,  OpCode::xorRV  },
            }); // clang-format on
        case Type::Float:
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::faddRV },
                { ArithmeticOperation::Sub,  OpCode::fsubRV },
                { ArithmeticOperation::Mul,  OpCode::fmulRV },
                { ArithmeticOperation::Div,  OpCode::fdivRV },
                { ArithmeticOperation::Rem,  OpCode::_count },
                { ArithmeticOperation::LShL, OpCode::_count },
                { ArithmeticOperation::LShR, OpCode::_count },
                { ArithmeticOperation::AShL, OpCode::_count },
                { ArithmeticOperation::AShR, OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            }); // clang-format on
        case Type::_count:
            SC_UNREACHABLE();
        }
    }
    if (dest == ValueType::RegisterIndex && source == ValueType::MemoryAddress)
    {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRM  },
                { ArithmeticOperation::Sub,  OpCode::subRM  },
                { ArithmeticOperation::Mul,  OpCode::mulRM  },
                { ArithmeticOperation::Div,  OpCode::idivRM },
                { ArithmeticOperation::Rem,  OpCode::iremRM },
                { ArithmeticOperation::LShL, OpCode::lslRM  },
                { ArithmeticOperation::LShR, OpCode::lsrRM  },
                { ArithmeticOperation::AShL, OpCode::aslRM  },
                { ArithmeticOperation::AShR, OpCode::asrRM  },
                { ArithmeticOperation::And,  OpCode::andRM  },
                { ArithmeticOperation::Or,   OpCode::orRM   },
                { ArithmeticOperation::XOr,  OpCode::xorRM  },
            }); // clang-format on
        case Type::Unsigned:
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRM  },
                { ArithmeticOperation::Sub,  OpCode::subRM  },
                { ArithmeticOperation::Mul,  OpCode::mulRM  },
                { ArithmeticOperation::Div,  OpCode::divRM  },
                { ArithmeticOperation::Rem,  OpCode::remRM  },
                { ArithmeticOperation::LShL, OpCode::lslRM  },
                { ArithmeticOperation::LShR, OpCode::lsrRM  },
                { ArithmeticOperation::AShL, OpCode::aslRM  },
                { ArithmeticOperation::AShR, OpCode::asrRM  },
                { ArithmeticOperation::And,  OpCode::andRM  },
                { ArithmeticOperation::Or,   OpCode::orRM   },
                { ArithmeticOperation::XOr,  OpCode::xorRM  },
            }); // clang-format on
        case Type::Float:
            // clang-format off
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::faddRM  },
                { ArithmeticOperation::Sub,  OpCode::fsubRM  },
                { ArithmeticOperation::Mul,  OpCode::fmulRM  },
                { ArithmeticOperation::Div,  OpCode::fdivRM  },
                { ArithmeticOperation::Rem,  OpCode::_count  },
                { ArithmeticOperation::LShL,  OpCode::_count },
                { ArithmeticOperation::LShR,  OpCode::_count },
                { ArithmeticOperation::AShL, OpCode::_count  },
                { ArithmeticOperation::AShR, OpCode::_count  },
                { ArithmeticOperation::And,  OpCode::_count  },
                { ArithmeticOperation::Or,   OpCode::_count  },
                { ArithmeticOperation::XOr,  OpCode::_count  },
            }); // clang-format on
        case Type::_count:
            SC_UNREACHABLE();
        }
    }
    /// No matching instruction.
    SC_DEBUGFAIL();
}

// clang-format on
