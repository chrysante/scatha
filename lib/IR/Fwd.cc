#include "IR/Fwd.h"

#include <ostream>

using namespace scatha;
using namespace ir;

std::string_view ir::toString(NodeType nodeType) {
    switch (nodeType) {
        // clang-format off
#define SC_VALUENODE_DEF(Node, _) case NodeType::Node: return #Node;
#include "IR/Lists.def"
        // clang-format on
    case NodeType::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

std::string_view ir::toString(Conversion conv) {
    switch (conv) {
        // clang-format off
#define SC_CONVERSION_DEF(Conv, Keyword) case Conversion::Conv: return #Keyword;
#include "IR/Lists.def"
        // clang-format on
    case Conversion::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, Conversion conv) {
    return ostream << toString(conv);
}

std::string_view ir::toString(CompareMode mode) {
    switch (mode) {
        // clang-format off
#define SC_COMPARE_MODE_DEF(Mode, name)                                        \
    case CompareMode::Mode: return #name;
#include "IR/Lists.def"
        // clang-format on
    case CompareMode::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareMode mode) {
    return ostream << toString(mode);
}

std::string_view ir::toString(CompareOperation op) {
    switch (op) {
        // clang-format off
#define SC_COMPARE_OPERATION_DEF(Op, name)                                     \
    case CompareOperation::Op: return #name;
#include "IR/Lists.def"
        // clang-format on
    case CompareOperation::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareOperation op) {
    return ostream << toString(op);
}

CompareOperation ir::inverse(CompareOperation compareOp) {
    // clang-format off
    return UTL_MAP_ENUM(compareOp, CompareOperation, {
        { CompareOperation::Less,      CompareOperation::GreaterEq },
        { CompareOperation::LessEq,    CompareOperation::Greater   },
        { CompareOperation::Greater,   CompareOperation::LessEq    },
        { CompareOperation::GreaterEq, CompareOperation::Less      },
        { CompareOperation::Equal,     CompareOperation::NotEqual  },
        { CompareOperation::NotEqual,  CompareOperation::Equal     },
    }); // clang-format on
}

std::string_view ir::toString(UnaryArithmeticOperation op) {
    switch (op) {
        // clang-format off
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Op, name)                            \
    case UnaryArithmeticOperation::Op: return #name;
#include "IR/Lists.def"
        // clang-format on
    case UnaryArithmeticOperation::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream,
                             UnaryArithmeticOperation op) {
    return ostream << toString(op);
}

std::string_view ir::toString(ArithmeticOperation op) {
    switch (op) {
        // clang-format off
#define SC_ARITHMETIC_OPERATION_DEF(Op, name)                                  \
    case ArithmeticOperation::Op: return #name;
#include "IR/Lists.def"
        // clang-format on
    case ArithmeticOperation::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, ArithmeticOperation op) {
    return ostream << toString(op);
}

bool ir::isShift(ArithmeticOperation op) {
    return op == ArithmeticOperation::LShL || op == ArithmeticOperation::LShR ||
           op == ArithmeticOperation::AShL || op == ArithmeticOperation::AShR;
}

bool ir::isCommutative(ArithmeticOperation op) {
    using enum ArithmeticOperation;
    switch (op) {
    case Add:
    case Mul:
    case And:
    case Or:
    case XOr:
    case FAdd:
    case FMul:
        return true;

    default:
        return false;
    }
    SC_UNREACHABLE();
}
