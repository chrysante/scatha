#include "Assembly2/Map.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace asm2;

using vm::OpCode;

// clang-format off

OpCode asm2::mapMove(ElemType dest, ElemType source) {
    if (dest == ElemType::RegisterIndex) {
        switch (source) {
        case ElemType::RegisterIndex:
            return OpCode::movRR;
        case ElemType::Value64:
            return OpCode::movRV;
        case ElemType::MemoryAddress:
            return OpCode::movRM;
        default:
            SC_DEBUGFAIL(); // No matching instruction
        }
    }
    if (dest == ElemType::MemoryAddress && source == ElemType::RegisterIndex) {
        return OpCode::movMR;
    }
    SC_DEBUGFAIL(); // No matching instruction
}

OpCode asm2::mapJump(CompareOperation condition) {
    return UTL_MAP_ENUM(condition, OpCode, {
        { CompareOperation::None,      OpCode::jmp },
        { CompareOperation::Less,      OpCode::jl  },
        { CompareOperation::LessEq,    OpCode::jle },
        { CompareOperation::Greater,   OpCode::jg  },
        { CompareOperation::GreaterEq, OpCode::jge },
        { CompareOperation::Eq,        OpCode::je  },
        { CompareOperation::NotEq,     OpCode::jne },
    });
}

OpCode asm2::mapCompare(Type type, ElemType lhs, ElemType rhs) {
    if (lhs == ElemType::RegisterIndex && rhs == ElemType::RegisterIndex) {
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::icmpRR },
            { Type::Unsigned, OpCode::ucmpRR },
            { Type::Float,    OpCode::fcmpRR },
        });
    }
    if (lhs == ElemType::RegisterIndex && rhs == ElemType::Value64) {
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::icmpRV },
            { Type::Unsigned, OpCode::ucmpRV },
            { Type::Float,    OpCode::fcmpRV },
        });
    }
    SC_DEBUGFAIL(); // No matching instruction
}

OpCode asm2::mapTest(Type type) {
    return UTL_MAP_ENUM(type, OpCode, {
        { Type::Signed,   OpCode::itest  },
        { Type::Unsigned, OpCode::utest  },
        { Type::Float,    OpCode::_count },
    });
}

OpCode asm2::mapSet(CompareOperation operation) {
    return UTL_MAP_ENUM(operation, OpCode, {
        { CompareOperation::None,      OpCode::_count },
        { CompareOperation::Less,      OpCode::setl  },
        { CompareOperation::LessEq,    OpCode::setle },
        { CompareOperation::Greater,   OpCode::setg  },
        { CompareOperation::GreaterEq, OpCode::setge },
        { CompareOperation::Eq,        OpCode::sete  },
        { CompareOperation::NotEq,     OpCode::setne },
    });
}

OpCode asm2::mapArithmetic(ArithmeticOperation operation, Type type, ElemType lhs, ElemType rhs) {
    if (lhs == ElemType::RegisterIndex && rhs == ElemType::RegisterIndex) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRR  },
                { ArithmeticOperation::Sub,  OpCode::subRR  },
                { ArithmeticOperation::Mul,  OpCode::mulRR  },
                { ArithmeticOperation::Div,  OpCode::idivRR },
                { ArithmeticOperation::Rem,  OpCode::iremRR },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::Unsigned:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRR  },
                { ArithmeticOperation::Sub,  OpCode::subRR  },
                { ArithmeticOperation::Mul,  OpCode::mulRR  },
                { ArithmeticOperation::Div,  OpCode::divRR  },
                { ArithmeticOperation::Rem,  OpCode::remRR  },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::Float:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::faddRR },
                { ArithmeticOperation::Sub,  OpCode::fsubRR },
                { ArithmeticOperation::Mul,  OpCode::fmulRR },
                { ArithmeticOperation::Div,  OpCode::fdivRR },
                { ArithmeticOperation::Rem,  OpCode::_count },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::_count: SC_UNREACHABLE();
        }
    }
    if (lhs == ElemType::RegisterIndex && rhs == ElemType::Value64) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRV  },
                { ArithmeticOperation::Sub,  OpCode::subRV  },
                { ArithmeticOperation::Mul,  OpCode::mulRV  },
                { ArithmeticOperation::Div,  OpCode::idivRV },
                { ArithmeticOperation::Rem,  OpCode::iremRV },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::Unsigned:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRV  },
                { ArithmeticOperation::Sub,  OpCode::subRV  },
                { ArithmeticOperation::Mul,  OpCode::mulRV  },
                { ArithmeticOperation::Div,  OpCode::divRV  },
                { ArithmeticOperation::Rem,  OpCode::remRV  },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::Float:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::faddRV },
                { ArithmeticOperation::Sub,  OpCode::fsubRV },
                { ArithmeticOperation::Mul,  OpCode::fmulRV },
                { ArithmeticOperation::Div,  OpCode::fdivRV },
                { ArithmeticOperation::Rem,  OpCode::_count },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::_count: SC_UNREACHABLE();
        }
    }
    if (lhs == ElemType::RegisterIndex && rhs == ElemType::MemoryAddress) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRM  },
                { ArithmeticOperation::Sub,  OpCode::subRM  },
                { ArithmeticOperation::Mul,  OpCode::mulRM  },
                { ArithmeticOperation::Div,  OpCode::idivRM },
                { ArithmeticOperation::Rem,  OpCode::iremRM },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::Unsigned:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRM  },
                { ArithmeticOperation::Sub,  OpCode::subRM  },
                { ArithmeticOperation::Mul,  OpCode::mulRM  },
                { ArithmeticOperation::Div,  OpCode::divRM  },
                { ArithmeticOperation::Rem,  OpCode::remRM  },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::Float:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::faddRM },
                { ArithmeticOperation::Sub,  OpCode::fsubRM },
                { ArithmeticOperation::Mul,  OpCode::fmulRM },
                { ArithmeticOperation::Div,  OpCode::fdivRM },
                { ArithmeticOperation::Rem,  OpCode::_count },
                { ArithmeticOperation::ShL,  OpCode::_count },
                { ArithmeticOperation::ShR,  OpCode::_count },
                { ArithmeticOperation::And,  OpCode::_count },
                { ArithmeticOperation::Or,   OpCode::_count },
                { ArithmeticOperation::XOr,  OpCode::_count },
            });
        case Type::_count: SC_UNREACHABLE();
        }
    }
    SC_DEBUGFAIL(); // No matching instruction.
}

// clang-format on
