#include "AST/Common.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

using namespace scatha;
using namespace ast;

std::string_view ast::toString(NodeType t) {
    return std::array {
#define SC_ASTNODE_DEF(node) std::string_view(#node),
#include "AST/Lists.def"
    }[static_cast<size_t>(t)];
}

std::ostream& ast::operator<<(std::ostream& str, NodeType t) {
    return str << toString(t);
}

bool ast::isDeclaration(NodeType t) {
    using enum NodeType;
    return t == FunctionDefinition || t == StructDefinition || t == VariableDeclaration;
}

std::string_view ast::toString(UnaryPrefixOperator op) {
    return std::array {
#define SC_UNARY_OPERATOR_DEF(name, opStr) std::string_view(opStr),
#include "AST/Lists.def"
    }[static_cast<size_t>(op)];
}

std::ostream& ast::operator<<(std::ostream& str, UnaryPrefixOperator op) {
    return str << toString(op);
}

std::string_view ast::toString(BinaryOperator op) {
    return std::array {
#define SC_BINARY_OPERATOR_DEF(name, opStr) std::string_view(opStr),
#include "AST/Lists.def"
    }[static_cast<size_t>(op)];
}

BinaryOperator ast::toNonAssignment(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::AddAssignment: return BinaryOperator::Addition;
    case BinaryOperator::SubAssignment: return BinaryOperator::Subtraction;
    case BinaryOperator::MulAssignment: return BinaryOperator::Multiplication;
    case BinaryOperator::DivAssignment: return BinaryOperator::Division;
    case BinaryOperator::RemAssignment: return BinaryOperator::Remainder;
    case BinaryOperator::LSAssignment: return BinaryOperator::LeftShift;
    case BinaryOperator::RSAssignment: return BinaryOperator::RightShift;
    case BinaryOperator::AndAssignment: return BinaryOperator::BitwiseAnd;
    case BinaryOperator::OrAssignment: return BinaryOperator::BitwiseOr;
    case BinaryOperator::XOrAssignment: return BinaryOperator::BitwiseXOr;
    default: SC_UNREACHABLE("'op' must be arithmetic assignment operator");
    }
}

bool ast::isArithmetic(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Multiplication: [[fallthrough]];
    case BinaryOperator::Division: [[fallthrough]];
    case BinaryOperator::Remainder: [[fallthrough]];
    case BinaryOperator::Addition: [[fallthrough]];
    case BinaryOperator::Subtraction: [[fallthrough]];
    case BinaryOperator::LeftShift: [[fallthrough]];
    case BinaryOperator::RightShift: [[fallthrough]];
    case BinaryOperator::BitwiseAnd: [[fallthrough]];
    case BinaryOperator::BitwiseXOr: [[fallthrough]];
    case BinaryOperator::BitwiseOr:
        return true;
    default:
        return false;
    }
}

bool ast::isLogical(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::LogicalAnd: [[fallthrough]];
    case BinaryOperator::LogicalOr:
        return true;
    default:
        return false;
    }
}

bool ast::isComparison(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Less: [[fallthrough]];
    case BinaryOperator::LessEq: [[fallthrough]];
    case BinaryOperator::Greater: [[fallthrough]];
    case BinaryOperator::GreaterEq: [[fallthrough]];
    case BinaryOperator::Equals: [[fallthrough]];
    case BinaryOperator::NotEquals:
        return true;
    default:
        return false;
    }
}

bool ast::isAssignment(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Assignment: [[fallthrough]];
    case BinaryOperator::AddAssignment: [[fallthrough]];
    case BinaryOperator::SubAssignment: [[fallthrough]];
    case BinaryOperator::MulAssignment: [[fallthrough]];
    case BinaryOperator::DivAssignment: [[fallthrough]];
    case BinaryOperator::RemAssignment: [[fallthrough]];
    case BinaryOperator::LSAssignment: [[fallthrough]];
    case BinaryOperator::RSAssignment: [[fallthrough]];
    case BinaryOperator::AndAssignment: [[fallthrough]];
    case BinaryOperator::OrAssignment: [[fallthrough]];
    case BinaryOperator::XOrAssignment:
        return true;
    default:
        return false;
    }
}

std::ostream& ast::operator<<(std::ostream& str, BinaryOperator op) {
    return str << toString(op);
}

std::ostream& ast::operator<<(std::ostream& str, EntityCategory k) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(k, {
        { EntityCategory::Value, "Value" },
        { EntityCategory::Type,  "Type" },
    });
    // clang-format off
}
