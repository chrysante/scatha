#pragma once

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <string>

#include <utl/vector.hpp>

#include "AST/Base.h"
#include "AST/Common.h"
#include "Sema/Scope.h"
#include "Sema/ScopeKind.h"
#include "Sema/SymbolID.h"

/// \code
/// AbstractSyntaxTree
/// +- ErrorNode
/// +- TranslationUnit
/// +- Statement
/// |  +- Declaration
/// |  |  +- VariableDeclaration
/// |  |  +- ParameterDeclaration
/// |  |  +- ModuleDeclaration
/// |  |  +- FunctionDefinition
/// |  |  +- StructDefinition
/// |  +- Block
/// |  +- ExpressionStatement
/// |  +- ControlFlowStatement
/// |     +- ReturnStatement
/// |     +- IfStatement
/// |     +- WhileStatement
/// +- Expression
///    +- Identifier
///    +- IntegerLiteral
///    +- StringLiteral
///    +- UnaryPrefixExpression
///    +- BinaryExpression
///    +- MemberAccess
///    +- Conditional
///    +- FunctionCall
///    +- Subscript
/// \endcode

namespace scatha::ast {

/// MARK: Expressions

/// Abstract node representing any expression.
struct SCATHA(API) Expression: AbstractSyntaxTree {
    using AbstractSyntaxTree::AbstractSyntaxTree;
    
    bool isValue() const { return category == EntityCategory::Value; }
    bool isType() const { return category == EntityCategory::Type; }
    
    /** Decoration provided by semantic analysis. */
    
    /// Wether the expression refers to a value or a type.
    EntityCategory category = EntityCategory::Value;
    
    /// The type of the expression. Only valid if kind == ::Value
    sema::TypeID typeID{};
};

/// Concrete node representing an identifier in an expression.
/// Identifier must refer to a value meaning a variable or a function.
struct SCATHA(API) Identifier: Expression {
    explicit Identifier(Token const& token): Expression(NodeType::Identifier, token) {}
    
    std::string_view value() const { return token().id; }
    
    /** Decoration provided by semantic analysis. */
    sema::SymbolID symbolID;
};

struct SCATHA(API) IntegerLiteral: Expression {
    explicit IntegerLiteral(Token const& token):
    Expression(NodeType::IntegerLiteral, token), value(token.toInteger()) {}
    
    u64 value;
};

struct SCATHA(API) BooleanLiteral: Expression {
    explicit BooleanLiteral(Token const& token): Expression(NodeType::BooleanLiteral, token), value(token.toBool()) {}
    
    bool value;
};

struct SCATHA(API) FloatingPointLiteral: Expression {
    explicit FloatingPointLiteral(Token const& token):
    Expression(NodeType::FloatingPointLiteral, token), value(token.toFloat()) {}
    
    f64 value;
};

struct SCATHA(API) StringLiteral: Expression {
    explicit StringLiteral(Token const& token): Expression(NodeType::StringLiteral, token), value(token.id) {}
    
    std::string value;
};

/// MARK: Unary Expressions
struct SCATHA(API) UnaryPrefixExpression: Expression {
    explicit UnaryPrefixExpression(UnaryPrefixOperator op, UniquePtr<Expression> operand, Token const& token):
    Expression(NodeType::UnaryPrefixExpression, token), op(op), operand(std::move(operand)) {}
    
    UnaryPrefixOperator op;
    UniquePtr<Expression> operand;
};

/// MARK: Binary Expressions
struct SCATHA(API) BinaryExpression: Expression {
    explicit BinaryExpression(BinaryOperator op,
                              UniquePtr<Expression> lhs,
                              UniquePtr<Expression> rhs,
                              Token const& token):
    Expression(NodeType::BinaryExpression, token), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    
    BinaryOperator op;
    UniquePtr<Expression> lhs;
    UniquePtr<Expression> rhs;
};

struct SCATHA(API) MemberAccess: Expression {
    explicit MemberAccess(UniquePtr<Expression> object, UniquePtr<Expression> member, Token const& dotToken):
    Expression(NodeType::MemberAccess, dotToken), object(std::move(object)), member(std::move(member)) {}
    
    UniquePtr<Expression> object;
    UniquePtr<Expression> member;
    sema::SymbolID symbolID;
};

/// MARK: Ternary Expressions
struct SCATHA(API) Conditional: Expression {
    explicit Conditional(UniquePtr<Expression> condition,
                         UniquePtr<Expression> ifExpr,
                         UniquePtr<Expression> elseExpr,
                         Token const& token):
    Expression(NodeType::Conditional, token),
    condition(std::move(condition)),
    ifExpr(std::move(ifExpr)),
    elseExpr(std::move(elseExpr)) {}
    
    UniquePtr<Expression> condition;
    UniquePtr<Expression> ifExpr;
    UniquePtr<Expression> elseExpr;
};

/// MARK: More Complex Expressions
struct SCATHA(API) FunctionCall: Expression {
    explicit FunctionCall(UniquePtr<Expression> object, Token const& token):
    Expression(NodeType::FunctionCall, token), object(std::move(object)) {}
    
    UniquePtr<Expression> object;
    utl::small_vector<UniquePtr<Expression>> arguments;
    
    /** Decoration provided by semantic analysis. */
    
    /// The SymbolID of the resolved function.
    /// Differs from the SymbolID of the object because the latter refers to an
    /// overload set or an object and this ID is resolved by overload
    /// resolution.
    sema::SymbolID functionID;
};

struct SCATHA(API) Subscript: Expression {
    explicit Subscript(UniquePtr<Expression> object, Token const& token):
    Expression(NodeType::Subscript, token), object(std::move(object)) {}
    
    UniquePtr<Expression> object;
    utl::small_vector<UniquePtr<Expression>> arguments;
};

/// Abstract node representing any statement.
struct SCATHA(API) Statement: AbstractSyntaxTree {
    using AbstractSyntaxTree::AbstractSyntaxTree;
};

/// Abstract node representing any declaration.
struct SCATHA(API) Declaration: Statement {
public:
    /// Name of the declared symbol as written in the source code.
    std::string_view name() const { return nameIdentifier ? std::string_view(nameIdentifier->token().id) : ""; }
    
    UniquePtr<Identifier> nameIdentifier;
    
    /// ** Decoration provided by semantic analysis **
    
    /// SymbolID of this declaration in the symbol table
    sema::SymbolID symbolID;
    
protected:
    explicit Declaration(NodeType type, Token const& declarator, UniquePtr<Identifier> name):
        Statement(type, declarator), nameIdentifier(std::move(name)) {}
};

/// Concrete node representing a translation unit.
struct SCATHA(API) TranslationUnit: AbstractSyntaxTree {
    TranslationUnit(): AbstractSyntaxTree(NodeType::TranslationUnit, Token{}) {}
    
    /// List of declarations in the translation unit.
    utl::small_vector<UniquePtr<Declaration>> declarations;
};

/// Concrete node representing a variable declaration.
struct SCATHA(API) VariableDeclaration: Declaration {
    explicit VariableDeclaration(Token const& declarator, UniquePtr<Identifier> name):
        Declaration(NodeType::VariableDeclaration, declarator, std::move(name)) {}
    
    bool isConstant             : 1 = false; // Will be set by the parser
    bool isFunctionParameter    : 1 = false; // Will be set by the parser
    bool isFunctionParameterDef : 1 = false; // Will be set by the constructor of
                                             // FunctionDefinition during parsing
    
    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;
    
    /// Expression to initialize this variable.
    UniquePtr<Expression> initExpression;
    
    /** Decoration provided by semantic analysis. */
    
    /// Type of the variable.
    /// Either deduced by the type of initExpression or by declTypename and then
    /// checked against the type of  initExpression
    sema::TypeID typeID = sema::TypeID::Invalid;
    
    /// Offset of the variable if this is a struct member
    size_t offset = 0;
};

/// Concrete node representing a variable declaration.
struct SCATHA(API) ParameterDeclaration: Declaration {
    explicit ParameterDeclaration(UniquePtr<Identifier> name, UniquePtr<Expression> typeExpr):
        Declaration(NodeType::ParameterDeclaration, Token{}, std::move(name)),
        typeExpr(std::move(typeExpr))
    {
        _token = this->nameIdentifier->token();
    }
    
    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;
    
    /** Decoration provided by semantic analysis. */
    
    /// Type of the variable.
    /// Either deduced by the type of initExpression or by declTypename and then
    /// checked against the type of  initExpression
    sema::TypeID typeID = sema::TypeID::Invalid;
};

/// Nothing to see here yet...
struct SCATHA(API) ModuleDeclaration: Declaration {
    ModuleDeclaration() = delete;
};

/// Concrete node representing any block like a function or loop body. Declares
/// its own (maybe anonymous) scope.
struct SCATHA(API) Block: Statement {
    explicit Block(Token const& token): Statement(NodeType::Block, token) {}
    
    /// Statements in the block.
    utl::small_vector<UniquePtr<Statement>> statements;
    
    /** Decoration provided by semantic analysis. */
    
    /// Kind of this block scope. Default is 'Anonymous', make sure to set this to Function or ObjectType in other
    /// cases.
    sema::ScopeKind scopeKind = sema::ScopeKind::Anonymous;
    
    /// SymbolID of this block scope
    sema::SymbolID scopeSymbolID{};
};

/// Concrete node representing an empty statement (";").
/// Note: This class exists so we don't have to ignore empty statements while parsing but can potentially handle them in some way in semantic analysis.
struct SCATHA(API) EmptyStatement: Statement {
    explicit EmptyStatement(Token const& token):
    Statement(NodeType::EmptyStatement, token) {}
};

/// Concrete node representing the definition of a function.
struct SCATHA(API) FunctionDefinition: Declaration {
    explicit FunctionDefinition(Token const& declarator, UniquePtr<Identifier> name):
        Declaration(NodeType::FunctionDefinition, std::move(declarator), std::move(name)) {}
    
    /// Typename of the return type as declared in the source code.
    /// Will be implicitly Identifier("void") if no return type was declared.
    UniquePtr<Expression> returnTypeExpr;
    
    /// List of parameter declarations.
    utl::small_vector<UniquePtr<ParameterDeclaration>> parameters;
    
    /// Body of the function.
    UniquePtr<Block> body;
    
    /** Decoration provided by semantic analysis. */
    
    /// Return type of the function.
    sema::TypeID returnTypeID = sema::TypeID::Invalid;
    
    /// Type of the function.
    sema::TypeID functionTypeID = sema::TypeID::Invalid;
};

/// Concrete node representing the definition of a struct.
struct SCATHA(API) StructDefinition: Declaration {
    explicit StructDefinition(Token const& declarator, UniquePtr<Identifier> name, UniquePtr<Block> body):
        Declaration(NodeType::StructDefinition, std::move(declarator), std::move(name)), body(std::move(body)) {}
    
    /// Body of the struct.
    UniquePtr<Block> body;
};

/// Concrete node representing a statement that consists of a single expression.
/// May only appear at function scope.
struct SCATHA(API) ExpressionStatement: Statement {
    explicit ExpressionStatement(UniquePtr<Expression> expression):
        Statement(NodeType::ExpressionStatement,
                  expression ? expression->token() : Token{}),
        expression(std::move(expression))
    {}
    
    /// The expression
    UniquePtr<Expression> expression;
};

/// Abstract node representing any control flow statement like if, while, for,
/// return etc. May only appear at function scope.
struct SCATHA(API) ControlFlowStatement: Statement {
protected:
    using Statement::Statement;
};

/// Concrete node representing a return statement.
struct SCATHA(API) ReturnStatement: ControlFlowStatement {
    explicit ReturnStatement(Token const& returnToken, UniquePtr<Expression> expression):
        ControlFlowStatement(NodeType::ReturnStatement, returnToken), expression(std::move(expression)) {}
    
    /// The returned expression. May be null in case of a void function.
    UniquePtr<Expression> expression;
};

/// Concrete node representing an if/else statement.
struct SCATHA(API) IfStatement: ControlFlowStatement {
    explicit IfStatement(Token const& token, UniquePtr<Expression> condition, UniquePtr<Statement> ifBlock, UniquePtr<Statement> elseBlock):
        ControlFlowStatement(NodeType::IfStatement, token), condition(std::move(condition)), ifBlock(std::move(ifBlock)), elseBlock(std::move(elseBlock)) {}
    
    /// Condition to branch on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;
    UniquePtr<Statement> ifBlock;
    UniquePtr<Statement> elseBlock;
};

/// Concrete node representing a while statement.
struct SCATHA(API) WhileStatement: ControlFlowStatement {
    explicit WhileStatement(Token const& token, UniquePtr<Expression> condition, UniquePtr<Block> block):
        ControlFlowStatement(NodeType::WhileStatement, token), condition(std::move(condition)), block(std::move(block)) {}
    
    /// Condition to loop on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;
    UniquePtr<Block> block;
};

} // namespace scatha::ast

#endif // SCATHA_AST_AST_H_
