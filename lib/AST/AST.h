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

/// AbstractSyntaxTree
/// +- TranslationUnit
/// +- Statement
/// |  +- Declaration
/// |  |  +- VariableDeclaration
/// |  |  +- ParameterDeclaration
/// |  |  +- ModuleDeclaration
/// |  |  +- FunctionDefinition
/// |  |  +- StructDefinition
/// |  +- CompoundStatement
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

namespace scatha::ast {

/// MARK: Expressions

/// Abstract node representing any expression.
class SCATHA(API) Expression: public AbstractSyntaxTree {
public:
    using AbstractSyntaxTree::AbstractSyntaxTree;

    /// **Decoration provided by semantic analysis**
        
    /// Wether the expression refers to a value or a type.
    EntityCategory category() const { expectDecorated(); return _category; }
    
    /// The type of the expression. Only valid if \code kind == ::Value \endcode
    sema::TypeID typeID() const { expectDecorated(); return _typeID; }
    
    bool isValue() const { return category() == EntityCategory::Value; }
    
    bool isType() const { return category() == EntityCategory::Type; }
    
    /// Decorate this node.
    void decorate(sema::TypeID typeID, EntityCategory category = EntityCategory::Value) {
        _category = category;
        _typeID = typeID;
        markDecorated();
    }
    
private:
    EntityCategory _category = EntityCategory::Value;
    sema::TypeID _typeID{};
};

/// Concrete node representing an identifier in an expression.
/// Identifier must refer to a value meaning a variable or a function.
class SCATHA(API) Identifier: public Expression {
public:
    explicit Identifier(Token const& token): Expression(NodeType::Identifier, token) {}

    std::string_view value() const { return token().id; }

    /// **Decoration provided by semantic analysis**

    /// ID of the resolved symbol
    sema::SymbolID symbolID() const { expectDecorated(); return _symbolID; }
    
    void decorate(sema::SymbolID symbolID, sema::TypeID typeID, EntityCategory category = EntityCategory::Value) {
        _symbolID = symbolID;
        Expression::decorate(typeID, category);
    }
    
private:
    sema::SymbolID _symbolID;
};

class SCATHA(API) IntegerLiteral: public Expression {
public:
    explicit IntegerLiteral(Token const& token):
        Expression(NodeType::IntegerLiteral, token), _value(token.toInteger()) {}

    u64 value() const { return _value; };
    
private:
    u64 _value;
};

class SCATHA(API) BooleanLiteral: public Expression {
public:
    explicit BooleanLiteral(Token const& token): Expression(NodeType::BooleanLiteral, token), _value(token.toBool()) {}

    bool value() const { return _value; }
    
private:
    bool _value;
};

class SCATHA(API) FloatingPointLiteral: public Expression {
public:
    explicit FloatingPointLiteral(Token const& token):
        Expression(NodeType::FloatingPointLiteral, token), _value(token.toFloat()) {}

    f64 value() const { return _value; }
    
private:
    f64 _value;
};

class SCATHA(API) StringLiteral: public Expression {
public:
    explicit StringLiteral(Token const& token): Expression(NodeType::StringLiteral, token), _value(token.id) {}

    std::string value() const { return _value; }
    
private:
    std::string _value;
};

/// MARK: Unary Expressions
class SCATHA(API) UnaryPrefixExpression: public Expression {
public:
    explicit UnaryPrefixExpression(UnaryPrefixOperator op, UniquePtr<Expression> operand, Token const& token):
        Expression(NodeType::UnaryPrefixExpression, token), op(op), operand(std::move(operand)) {}

    UnaryPrefixOperator op;
    UniquePtr<Expression> operand;
};

/// MARK: Binary Expressions
class SCATHA(API) BinaryExpression: public Expression {
public:
    explicit BinaryExpression(BinaryOperator op,
                              UniquePtr<Expression> lhs,
                              UniquePtr<Expression> rhs,
                              Token const& token):
        Expression(NodeType::BinaryExpression, token), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    BinaryOperator op;
    UniquePtr<Expression> lhs;
    UniquePtr<Expression> rhs;
};

class SCATHA(API) MemberAccess: public Expression {
public:
    explicit MemberAccess(UniquePtr<Expression> object, UniquePtr<Expression> member, Token const& dotToken):
        Expression(NodeType::MemberAccess, dotToken), object(std::move(object)), member(std::move(member)) {}

    UniquePtr<Expression> object;
    UniquePtr<Expression> member;

    /// **Decoration provided by semantic analysis**
    
    /// ID of the resolved symbol
    sema::SymbolID symbolID() const { expectDecorated(); return _symbolID; }
    
    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::TypeID typeID, EntityCategory category = EntityCategory::Value) {
        _symbolID = symbolID;
        Expression::decorate(typeID, category);
    }

private:
    sema::SymbolID _symbolID;
};

/// MARK: Ternary Expressions
class SCATHA(API) Conditional: public Expression {
public:
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
class SCATHA(API) FunctionCall: public Expression {
public:
    explicit FunctionCall(UniquePtr<Expression> object, Token const& token):
        Expression(NodeType::FunctionCall, token), object(std::move(object)) {}

    UniquePtr<Expression> object;
    utl::small_vector<UniquePtr<Expression>> arguments;

    /// **Decoration provided by semantic analysis**

    /// The SymbolID of the resolved function.
    /// Differs from the SymbolID of the object because the latter refers to an
    /// overload set or an object and this ID is resolved by overload
    /// resolution.
    sema::SymbolID functionID() const { expectDecorated(); return _functionID; }
    
    /// Decorate this node.
    void decorate(sema::SymbolID functionID, sema::TypeID typeID, EntityCategory category = EntityCategory::Value) {
        _functionID = functionID;
        Expression::decorate(typeID, category);
    }
    
private:
    sema::SymbolID _functionID;
};

class SCATHA(API) Subscript: public Expression {
public:
    explicit Subscript(UniquePtr<Expression> object, Token const& token):
        Expression(NodeType::Subscript, token), object(std::move(object)) {}

    UniquePtr<Expression> object;
    utl::small_vector<UniquePtr<Expression>> arguments;
};

/// Abstract node representing a statement.
class SCATHA(API) Statement: public AbstractSyntaxTree {
public:
    using AbstractSyntaxTree::AbstractSyntaxTree;
};

/// Abstract node representing a declaration.
class SCATHA(API) Declaration: public Statement {
public:
    /// Name of the declared symbol as written in the source code.
    std::string_view name() const { return nameIdentifier ? std::string_view(nameIdentifier->token().id) : ""; }

    UniquePtr<Identifier> nameIdentifier;

    /// **Decoration provided by semantic analysis**

    /// SymbolID of this declaration in the symbol table
    sema::SymbolID symbolID() const { expectDecorated(); return _symbolID; }
    
    /// Decorate this node.
    void decorate(sema::SymbolID symbolID) {
        _symbolID = symbolID;
        markDecorated();
    }

protected:
    explicit Declaration(NodeType type, Token const& declarator, UniquePtr<Identifier> name):
    Statement(type, declarator), nameIdentifier(std::move(name)) {}

private:
    sema::SymbolID _symbolID;
};

/// Concrete node representing a translation unit.
class SCATHA(API) TranslationUnit: public AbstractSyntaxTree {
public:
    TranslationUnit(): AbstractSyntaxTree(NodeType::TranslationUnit, Token{}) {}

    /// List of declarations in the translation unit.
    utl::small_vector<UniquePtr<Declaration>> declarations;
};

/// Concrete node representing a variable declaration.
class SCATHA(API) VariableDeclaration: public Declaration {
public:
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

    /// **Decoration provided by semantic analysis**

    /// Type of the variable.
    /// Either deduced by the type of initExpression or by declTypename and then
    /// checked against the type of  initExpression
    sema::TypeID typeID() const { expectDecorated(); return _typeID; }
    
    /// Offset of the variable if this is a struct member
    size_t offset() const { expectDecorated(); return _offset; }
        
    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::TypeID typeID, size_t offset = 0) {
        _typeID = typeID;
        _offset = offset;
        Declaration::decorate(symbolID);
    }
    
    void setOffset(size_t offset) { _offset = offset; }

private:
    sema::TypeID _typeID = sema::TypeID::Invalid;
    size_t _offset       = 0;
};

/// Concrete node representing a parameter declaration.
class SCATHA(API) ParameterDeclaration: public Declaration {
public:
    explicit ParameterDeclaration(UniquePtr<Identifier> name, UniquePtr<Expression> typeExpr):
        Declaration(NodeType::ParameterDeclaration, Token{}, std::move(name)), typeExpr(std::move(typeExpr)) {
        _token = this->nameIdentifier->token();
    }

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// **Decoration provided by semantic analysis**

    /// Type of the parameter.
    sema::TypeID typeID() const { expectDecorated(); return _typeID; }
    
    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::TypeID typeID) {
        _typeID = typeID;
        Declaration::decorate(symbolID);
    }
    
private:
    sema::TypeID _typeID = sema::TypeID::Invalid;
};

/// Nothing to see here yet...
class SCATHA(API) ModuleDeclaration: public Declaration {
public:
    ModuleDeclaration() = delete;
};

/// Concrete node representing any block like a function or loop body. Declares
/// its own (maybe anonymous) scope.
class SCATHA(API) CompoundStatement: public Statement {
public:
    explicit CompoundStatement(Token const& token): Statement(NodeType::CompoundStatement, token) {}

    /// Statements in the block.
    utl::small_vector<UniquePtr<Statement>> statements;

    /// **Decoration provided by semantic analysis**

    /// Kind of this block scope. Default is 'Anonymous', make sure to set this to Function or ObjectType in other
    /// cases.
    sema::ScopeKind scopeKind() const { expectDecorated(); return _scopeKind; }
    
    /// SymbolID of this declaration in the symbol table
    sema::SymbolID symbolID() const { expectDecorated(); return _symbolID; }

    /// Decorate this node.
    void decorate(sema::ScopeKind scopeKind, sema::SymbolID symbolID) {
        _scopeKind = scopeKind;
        _symbolID = symbolID;
        markDecorated();
    }
    
private:
    sema::ScopeKind _scopeKind = sema::ScopeKind::Anonymous;
    sema::SymbolID _symbolID;
};

/// Concrete node representing an empty statement (";").
/// Note: This class exists so we don't have to ignore empty statements while parsing but can potentially handle them in
/// some way in semantic analysis.
class SCATHA(API) EmptyStatement: public Statement {
public:
    explicit EmptyStatement(Token const& token): Statement(NodeType::EmptyStatement, token) {}
};

/// Concrete node representing the definition of a function.
class SCATHA(API) FunctionDefinition: public Declaration {
public:
    explicit FunctionDefinition(Token const& declarator, UniquePtr<Identifier> name):
        Declaration(NodeType::FunctionDefinition, std::move(declarator), std::move(name)) {}

    /// Typename of the return type as declared in the source code.
    /// Will be NULL if no return type was declared.
    UniquePtr<Expression> returnTypeExpr;

    /// List of parameter declarations.
    utl::small_vector<UniquePtr<ParameterDeclaration>> parameters;

    /// Body of the function.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// Return type of the function.
    sema::TypeID returnTypeID() const { expectDecorated(); return _returnTypeID; }
    
    /// Type of the function.
    sema::TypeID functionTypeID() const { expectDecorated(); return _functionTypeID; }
    
    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::TypeID returnTypeID, sema::TypeID functionTypeID) {
        _returnTypeID = returnTypeID;
        _functionTypeID = functionTypeID;
        Declaration::decorate(symbolID);
    }
    
private:
    sema::TypeID _returnTypeID   = sema::TypeID::Invalid;
    sema::TypeID _functionTypeID = sema::TypeID::Invalid;
};

/// Concrete node representing the definition of a struct.
class SCATHA(API) StructDefinition: public Declaration {
public:
    explicit StructDefinition(Token const& declarator, UniquePtr<Identifier> name, UniquePtr<CompoundStatement> body):
        Declaration(NodeType::StructDefinition, std::move(declarator), std::move(name)), body(std::move(body)) {}

    /// Body of the struct.
    UniquePtr<CompoundStatement> body;
    
    /// **Decoration provided by semantic analysis**
    
    /// Decorate this node.
    void decorate(sema::SymbolID symbolID) {
        Declaration::decorate(symbolID);
    }
};

/// Concrete node representing a statement that consists of a single expression.
/// May only appear at function scope.
class SCATHA(API) ExpressionStatement: public Statement {
public:
    explicit ExpressionStatement(UniquePtr<Expression> expression):
        Statement(NodeType::ExpressionStatement, expression ? expression->token() : Token{}),
        expression(std::move(expression)) {}

    /// The expression
    UniquePtr<Expression> expression;
};

/// Abstract node representing any control flow statement like if, while, for,
/// return etc. May only appear at function scope.
class SCATHA(API) ControlFlowStatement: public Statement {
protected:
    using Statement::Statement;
};

/// Concrete node representing a return statement.
class SCATHA(API) ReturnStatement: public ControlFlowStatement {
public:
    explicit ReturnStatement(Token const& returnToken, UniquePtr<Expression> expression):
        ControlFlowStatement(NodeType::ReturnStatement, returnToken), expression(std::move(expression)) {}

    /// The returned expression. May be null in case of a void function.
    UniquePtr<Expression> expression;
};

/// Concrete node representing an if/else statement.
class SCATHA(API) IfStatement: public ControlFlowStatement {
public:
    explicit IfStatement(Token const& token,
                         UniquePtr<Expression> condition,
                         UniquePtr<Statement> ifBlock,
                         UniquePtr<Statement> elseBlock):
        ControlFlowStatement(NodeType::IfStatement, token),
        condition(std::move(condition)),
        ifBlock(std::move(ifBlock)),
        elseBlock(std::move(elseBlock)) {}

    /// Condition to branch on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;
    UniquePtr<Statement> ifBlock;
    UniquePtr<Statement> elseBlock;
};

/// Concrete node representing a while statement.
class SCATHA(API) WhileStatement: public ControlFlowStatement {
public:
    explicit WhileStatement(Token const& token, UniquePtr<Expression> condition, UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::WhileStatement, token),
        condition(std::move(condition)),
        block(std::move(block)) {}

    /// Condition to loop on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;
    UniquePtr<CompoundStatement> block;
};

} // namespace scatha::ast

#endif // SCATHA_AST_AST_H_
