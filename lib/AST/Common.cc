#include "AST/Common.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::ast {

bool isDeclaration(NodeType t) {
    using enum NodeType;
    return t == FunctionDefinition || t == StructDefinition || t == VariableDeclaration;
}

std::string_view toString(NodeType t) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(t, {
        { NodeType::TranslationUnit,       "TranslationUnit" },
        { NodeType::Block,                 "Block" },
        { NodeType::FunctionDefinition,    "FunctionDefinition" },
        { NodeType::StructDefinition,      "StructDefinition" },
        { NodeType::VariableDeclaration,   "VariableDeclaration" },
        { NodeType::ExpressionStatement,   "ExpressionStatement" },
        { NodeType::ReturnStatement,       "ReturnStatement" },
        { NodeType::IfStatement,           "IfStatement" },
        { NodeType::WhileStatement,        "WhileStatement" },
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

std::ostream& operator<<(std::ostream& str, NodeType t) {
    return str << toString(t);
}

std::string_view toString(UnaryPrefixOperator op) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(op, {
        { UnaryPrefixOperator::Promotion,  "+" },
        { UnaryPrefixOperator::Negation,   "-" },
        { UnaryPrefixOperator::BitwiseNot, "~" },
        { UnaryPrefixOperator::LogicalNot, "!" },
    });
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, UnaryPrefixOperator op) {
    return str << toString(op);
}

std::string_view toString(BinaryOperator op) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(op, {
        { BinaryOperator::Multiplication, "*" },
        { BinaryOperator::Division,       "/" },
        { BinaryOperator::Remainder,      "%" },
        { BinaryOperator::Addition,       "+" },
        { BinaryOperator::Subtraction,    "-" },
        { BinaryOperator::LeftShift,      "<<" },
        { BinaryOperator::RightShift,     ">>" },
        { BinaryOperator::Less,           "<" },
        { BinaryOperator::LessEq,         "<=" },
        { BinaryOperator::Greater,        ">" },
        { BinaryOperator::GreaterEq,      ">=" },
        { BinaryOperator::Equals,         "==" },
        { BinaryOperator::NotEquals,      "!=" },
        { BinaryOperator::BitwiseAnd,     "&" },
        { BinaryOperator::BitwiseXOr,     "^" },
        { BinaryOperator::BitwiseOr,      "|" },
        { BinaryOperator::LogicalAnd,     "&&" },
        { BinaryOperator::LogicalOr,      "||" },
        { BinaryOperator::Assignment,     "=" },
        { BinaryOperator::AddAssignment,  "+=" },
        { BinaryOperator::SubAssignment,  "-=" },
        { BinaryOperator::MulAssignment,  "*=" },
        { BinaryOperator::DivAssignment,  "/=" },
        { BinaryOperator::RemAssignment,  "%=" },
        { BinaryOperator::LSAssignment,   "<<=" },
        { BinaryOperator::RSAssignment,   ">>=" },
        { BinaryOperator::AndAssignment,  "&=" },
        { BinaryOperator::OrAssignment,   "|=" },
        { BinaryOperator::XOrAssignment,  "^=" },
        { BinaryOperator::Comma,          "," },
    });
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, BinaryOperator op) {
    return str << toString(op);
}

std::ostream& operator<<(std::ostream& str, EntityCategory k) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(k, {
        { EntityCategory::Value, "Value" },
        { EntityCategory::Type,  "Type" },
    });
    // clang-format off
}

} // namespace scatha::ast
