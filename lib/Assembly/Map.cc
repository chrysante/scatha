#include "Assembly/Map.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace Asm;

using vm::OpCode;

std::pair<OpCode, size_t> Asm::mapMove(ValueType dest, ValueType source, size_t size) {
    if (dest == ValueType::RegisterIndex) {
        switch (source) {
        case ValueType::RegisterIndex: return { OpCode::mov64RR, 8 };
        case ValueType::MemoryAddress:
            switch (size) {
            case 1: return { OpCode::mov8RM, 1 };
            case 2: return { OpCode::mov16RM, 2 };
            case 4: return { OpCode::mov32RM, 4 };
            case 8: return { OpCode::mov64RM, 8 };
            default: SC_UNREACHABLE();
            }
        case ValueType::Value8:  [[fallthrough]];
        case ValueType::Value16: [[fallthrough]];
        case ValueType::Value32: [[fallthrough]];
        case ValueType::Value64: return { OpCode::mov64RV, 8 };
        default: SC_DEBUGFAIL(); // No matching instruction
        }
    }
    if (dest == ValueType::MemoryAddress && source == ValueType::RegisterIndex) {
        switch (size) {
        case 1: return { OpCode::mov8MR, 1 };
        case 2: return { OpCode::mov16MR, 2 };
        case 4: return { OpCode::mov32MR, 4 };
        case 8: return { OpCode::mov64MR, 8 };
        default: SC_UNREACHABLE();
        }
    }
    SC_DEBUGFAIL(); // No matching instruction
}

// clang-format off

OpCode Asm::mapJump(CompareOperation condition) {
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

OpCode Asm::mapCompare(Type type, ValueType lhs, ValueType rhs) {
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::RegisterIndex) {
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::icmpRR },
            { Type::Unsigned, OpCode::ucmpRR },
            { Type::Float,    OpCode::fcmpRR },
        });
    }
    if (lhs == ValueType::RegisterIndex && rhs == ValueType::Value64) {
        return UTL_MAP_ENUM(type, OpCode, {
            { Type::Signed,   OpCode::icmpRV },
            { Type::Unsigned, OpCode::ucmpRV },
            { Type::Float,    OpCode::fcmpRV },
        });
    }
    SC_DEBUGFAIL(); // No matching instruction
}

OpCode Asm::mapTest(Type type) {
    return UTL_MAP_ENUM(type, OpCode, {
        { Type::Signed,   OpCode::itest  },
        { Type::Unsigned, OpCode::utest  },
        { Type::Float,    OpCode::_count },
    });
}

OpCode Asm::mapSet(CompareOperation operation) {
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

OpCode Asm::mapArithmetic(ArithmeticOperation operation, Type type, ValueType dest, ValueType source) {
    if (dest == ValueType::RegisterIndex && source == ValueType::RegisterIndex) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRR  },
                { ArithmeticOperation::Sub,  OpCode::subRR  },
                { ArithmeticOperation::Mul,  OpCode::mulRR  },
                { ArithmeticOperation::Div,  OpCode::idivRR },
                { ArithmeticOperation::Rem,  OpCode::iremRR },
                { ArithmeticOperation::ShL,  OpCode::slRR   },
                { ArithmeticOperation::ShR,  OpCode::srRR   },
                { ArithmeticOperation::And,  OpCode::andRR  },
                { ArithmeticOperation::Or,   OpCode::orRR   },
                { ArithmeticOperation::XOr,  OpCode::xorRR  },
            });
        case Type::Unsigned:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRR  },
                { ArithmeticOperation::Sub,  OpCode::subRR  },
                { ArithmeticOperation::Mul,  OpCode::mulRR  },
                { ArithmeticOperation::Div,  OpCode::divRR  },
                { ArithmeticOperation::Rem,  OpCode::remRR  },
                { ArithmeticOperation::ShL,  OpCode::slRR   },
                { ArithmeticOperation::ShR,  OpCode::srRR   },
                { ArithmeticOperation::And,  OpCode::andRR  },
                { ArithmeticOperation::Or,   OpCode::orRR   },
                { ArithmeticOperation::XOr,  OpCode::xorRR  },
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
    if (dest == ValueType::RegisterIndex && source == ValueType::Value64) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRV  },
                { ArithmeticOperation::Sub,  OpCode::subRV  },
                { ArithmeticOperation::Mul,  OpCode::mulRV  },
                { ArithmeticOperation::Div,  OpCode::idivRV },
                { ArithmeticOperation::Rem,  OpCode::iremRV },
                { ArithmeticOperation::ShL,  OpCode::slRV   },
                { ArithmeticOperation::ShR,  OpCode::srRV   },
                { ArithmeticOperation::And,  OpCode::andRV  },
                { ArithmeticOperation::Or,   OpCode::orRV   },
                { ArithmeticOperation::XOr,  OpCode::xorRV  },
            });
        case Type::Unsigned:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRV  },
                { ArithmeticOperation::Sub,  OpCode::subRV  },
                { ArithmeticOperation::Mul,  OpCode::mulRV  },
                { ArithmeticOperation::Div,  OpCode::divRV  },
                { ArithmeticOperation::Rem,  OpCode::remRV  },
                { ArithmeticOperation::ShL,  OpCode::slRV   },
                { ArithmeticOperation::ShR,  OpCode::srRV   },
                { ArithmeticOperation::And,  OpCode::andRV  },
                { ArithmeticOperation::Or,   OpCode::orRV   },
                { ArithmeticOperation::XOr,  OpCode::xorRV  },
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
    if (dest == ValueType::RegisterIndex && source == ValueType::MemoryAddress) {
        switch (type) {
        case Type::Signed:
            // TODO: Take care of shift and bitwise operations.
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRM  },
                { ArithmeticOperation::Sub,  OpCode::subRM  },
                { ArithmeticOperation::Mul,  OpCode::mulRM  },
                { ArithmeticOperation::Div,  OpCode::idivRM },
                { ArithmeticOperation::Rem,  OpCode::iremRM },
                { ArithmeticOperation::ShL,  OpCode::slRM   },
                { ArithmeticOperation::ShR,  OpCode::srRM   },
                { ArithmeticOperation::And,  OpCode::andRM  },
                { ArithmeticOperation::Or,   OpCode::orRM   },
                { ArithmeticOperation::XOr,  OpCode::xorRM  },
            });
        case Type::Unsigned:
            return UTL_MAP_ENUM(operation, OpCode, {
                { ArithmeticOperation::Add,  OpCode::addRM  },
                { ArithmeticOperation::Sub,  OpCode::subRM  },
                { ArithmeticOperation::Mul,  OpCode::mulRM  },
                { ArithmeticOperation::Div,  OpCode::divRM  },
                { ArithmeticOperation::Rem,  OpCode::remRM  },
                { ArithmeticOperation::ShL,  OpCode::slRM   },
                { ArithmeticOperation::ShR,  OpCode::srRM   },
                { ArithmeticOperation::And,  OpCode::andRM  },
                { ArithmeticOperation::Or,   OpCode::orRM   },
                { ArithmeticOperation::XOr,  OpCode::xorRM  },
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
