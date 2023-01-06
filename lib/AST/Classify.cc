#include "Classify.h"

using namespace scatha;
using namespace ast;

bool ast::isDeclaration(NodeType t) {
    using enum NodeType;
    return t == FunctionDefinition || t == StructDefinition || t == VariableDeclaration;
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
    case BinaryOperator::BitwiseOr: return true;
    default: return false;
    }
}

bool ast::isLogical(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::LogicalAnd: [[fallthrough]];
    case BinaryOperator::LogicalOr: return true;
    default: return false;
    }
}

bool ast::isComparison(BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Less: [[fallthrough]];
    case BinaryOperator::LessEq: [[fallthrough]];
    case BinaryOperator::Greater: [[fallthrough]];
    case BinaryOperator::GreaterEq: [[fallthrough]];
    case BinaryOperator::Equals: [[fallthrough]];
    case BinaryOperator::NotEquals: return true;
    default: return false;
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
    case BinaryOperator::XOrAssignment: return true;
    default: return false;
    }
}
