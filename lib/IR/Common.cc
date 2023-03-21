#include "IR/Common.h"

#include <ostream>

using namespace scatha;
using namespace ir;

std::string_view ir::toString(NodeType nodeType) {
    switch (nodeType) {
        // clang-format off
#define SC_CGFNODE_DEF(Node, _) case NodeType::Node: return #Node;
#include "IR/Lists.def"
        // clang-format on
    case NodeType::_count:
        SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
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
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareOperation op) {
    return ostream << toString(op);
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
    };
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
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, ArithmeticOperation op) {
    return ostream << toString(op);
}
