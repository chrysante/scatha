#include "AST/Common.h"

#include <ostream>

#include <utl/utility.hpp>

using namespace scatha;
using namespace ast;

bool ast::isDeclaration(NodeType t) {
    using enum NodeType;
    return t == FunctionDefinition || t == StructDefinition || t == VariableDeclaration;
}

std::string_view ast::toString(NodeType t) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(t, {
        { NodeType::AbstractSyntaxTree,    "AbstractSyntaxTree" },
        { NodeType::TranslationUnit,       "TranslationUnit" },
        { NodeType::CompoundStatement,     "CompoundStatement" },
        { NodeType::Declaration,           "Declaration" },
        { NodeType::FunctionDefinition,    "FunctionDefinition" },
        { NodeType::StructDefinition,      "StructDefinition" },
        { NodeType::VariableDeclaration,   "VariableDeclaration" },
        { NodeType::ParameterDeclaration,  "ParameterDeclaration" },
        { NodeType::Statement,             "Statement" },
        { NodeType::ExpressionStatement,   "ExpressionStatement" },
        { NodeType::EmptyStatement,        "EmptyStatement" },
        { NodeType::ReturnStatement,       "ReturnStatement" },
        { NodeType::IfStatement,           "IfStatement" },
        { NodeType::WhileStatement,        "WhileStatement" },
        { NodeType::DoWhileStatement,      "DoWhileStatement" },
        { NodeType::Expression,            "Expression" },
        { NodeType::Identifier,            "Identifier" },
        { NodeType::IntegerLiteral,        "IntegerLiteral" },
        { NodeType::BooleanLiteral,        "BooleanLiteral" },
        { NodeType::FloatingPointLiteral,  "FloatingPointLiteral" },
        { NodeType::StringLiteral,         "StringLiteral" },
        { NodeType::UnaryPrefixExpression, "UnaryPrefixExpression" },
        { NodeType::BinaryExpression,      "BinaryExpression" },
        { NodeType::MemberAccess,          "MemberAccess" },
        { NodeType::Conditional,           "Conditional" },
        { NodeType::FunctionCall,          "FunctionCall" },
        { NodeType::Subscript,             "Subscript" },
    });
    // clang-format on
}

std::ostream& ast::operator<<(std::ostream& str, NodeType t) {
    return str << toString(t);
}

std::string_view ast::toString(UnaryPrefixOperator op) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(op, {
        { UnaryPrefixOperator::Promotion,  "+" },
        { UnaryPrefixOperator::Negation,   "-" },
        { UnaryPrefixOperator::BitwiseNot, "~" },
        { UnaryPrefixOperator::LogicalNot, "!" },
    });
    // clang-format on
}

std::ostream& ast::operator<<(std::ostream& str, UnaryPrefixOperator op) {
    return str << toString(op);
}

std::string_view ast::toString(BinaryOperator op) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(op, {
        { BinaryOperator::Multiplication, "*"   },
        { BinaryOperator::Division,       "/"   },
        { BinaryOperator::Remainder,      "%"   },
        { BinaryOperator::Addition,       "+"   },
        { BinaryOperator::Subtraction,    "-"   },
        { BinaryOperator::LeftShift,      "<<"  },
        { BinaryOperator::RightShift,     ">>"  },
        { BinaryOperator::Less,           "<"   },
        { BinaryOperator::LessEq,         "<="  },
        { BinaryOperator::Greater,        ">"   },
        { BinaryOperator::GreaterEq,      ">="  },
        { BinaryOperator::Equals,         "=="  },
        { BinaryOperator::NotEquals,      "!="  },
        { BinaryOperator::BitwiseAnd,     "&"   },
        { BinaryOperator::BitwiseXOr,     "^"   },
        { BinaryOperator::BitwiseOr,      "|"   },
        { BinaryOperator::LogicalAnd,     "&&"  },
        { BinaryOperator::LogicalOr,      "||"  },
        { BinaryOperator::Assignment,     "="   },
        { BinaryOperator::AddAssignment,  "+="  },
        { BinaryOperator::SubAssignment,  "-="  },
        { BinaryOperator::MulAssignment,  "*="  },
        { BinaryOperator::DivAssignment,  "/="  },
        { BinaryOperator::RemAssignment,  "%="  },
        { BinaryOperator::LSAssignment,   "<<=" },
        { BinaryOperator::RSAssignment,   ">>=" },
        { BinaryOperator::AndAssignment,  "&="  },
        { BinaryOperator::OrAssignment,   "|="  },
        { BinaryOperator::XOrAssignment,  "^="  },
        { BinaryOperator::Comma,          ","   },
    });
    // clang-format on
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
