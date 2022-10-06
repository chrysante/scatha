#pragma once

#ifndef SCATHA_AST_FWD_H_
#define SCATHA_AST_FWD_H_

namespace scatha::ast {

struct AbstractSyntaxTree;
struct TranslationUnit;
struct Block;
struct FunctionDefinition;
struct StructDefinition;
struct VariableDeclaration;
struct ExpressionStatement;
struct ReturnStatement;
struct IfStatement;
struct WhileStatement;
struct Identifier;
struct IntegerLiteral;
struct BooleanLiteral;
struct FloatingPointLiteral;
struct StringLiteral;
struct UnaryPrefixExpression;
struct BinaryExpression;
struct MemberAccess;
struct Conditional;
struct FunctionCall;
struct Subscript;

} // namespace scatha::ast

#endif // SCATHA_AST_FWD_H_
