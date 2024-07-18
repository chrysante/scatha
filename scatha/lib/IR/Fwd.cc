#include "IR/Fwd.h"

#include <ostream>

using namespace scatha;
using namespace ir;

std::string_view ir::toString(NodeType nodeType) {
    switch (nodeType) {
        // clang-format off
#define SC_VALUENODE_DEF(Node, ...) case NodeType::Node: return #Node;
#include "IR/Lists.def.h"
        // clang-format on
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

std::string ir::toString(AttributeType attrib) {
    switch (attrib) {
        // clang-format off
#define SC_ATTRIBUTE_DEF(Attrib, ...) case AttributeType::Attrib: return #Attrib;
#include "IR/Lists.def.h"
        // clang-format on
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, AttributeType attrib) {
    return ostream << toString(attrib);
}

std::string_view ir::toString(Conversion conv) {
    switch (conv) {
#define SC_CONVERSION_DEF(Conv, Keyword)                                       \
    case Conversion::Conv:                                                     \
        return #Keyword;
#include "IR/Lists.def.h"
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, Conversion conv) {
    return ostream << toString(conv);
}

std::string_view ir::toString(CompareMode mode) {
    switch (mode) {
#define SC_COMPARE_MODE_DEF(Mode, name)                                        \
    case CompareMode::Mode:                                                    \
        return #name;
#include "IR/Lists.def.h"
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareMode mode) {
    return ostream << toString(mode);
}

std::string_view ir::toString(CompareOperation op) {
    switch (op) {
#define SC_COMPARE_OPERATION_DEF(Op, name)                                     \
    case CompareOperation::Op:                                                 \
        return #name;
#include "IR/Lists.def.h"
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareOperation op) {
    return ostream << toString(op);
}

CompareOperation ir::inverse(CompareOperation compareOp) {
    switch (compareOp) {
    case CompareOperation::Less:
        return CompareOperation::GreaterEq;
    case CompareOperation::LessEq:
        return CompareOperation::Greater;
    case CompareOperation::Greater:
        return CompareOperation::LessEq;
    case CompareOperation::GreaterEq:
        return CompareOperation::Less;
    case CompareOperation::Equal:
        return CompareOperation::NotEqual;
    case CompareOperation::NotEqual:
        return CompareOperation::Equal;
    }
}

std::string_view ir::toString(UnaryArithmeticOperation op) {
    switch (op) {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Op, name)                            \
    case UnaryArithmeticOperation::Op:                                         \
        return #name;
#include "IR/Lists.def.h"
    }
    SC_UNREACHABLE();
}

std::ostream& ir::operator<<(std::ostream& ostream,
                             UnaryArithmeticOperation op) {
    return ostream << toString(op);
}

std::string_view ir::toString(ArithmeticOperation op) {
    switch (op) {
#define SC_ARITHMETIC_OPERATION_DEF(Op, name)                                  \
    case ArithmeticOperation::Op:                                              \
        return #name;
#include "IR/Lists.def.h"
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
