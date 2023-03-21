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
    SC_DEBUGFAIL(); // No matching instruction
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
    SC_DEBUGFAIL(); // No matching instruction
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
    SC_DEBUGFAIL(); // No matching instruction.
}

// clang-format on
