// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <string>

#include <utl/vector.hpp>

#include <scatha/AST/Base.h>
#include <scatha/AST/Common.h>
#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Sema/Scope.h>
#include <scatha/Sema/ScopeKind.h>
#include <scatha/Sema/SymbolID.h>

// AbstractSyntaxTree
// ├─ TranslationUnit
// ├─ Statement
// │  ├─ Declaration
// │  │  ├─ VariableDeclaration
// │  │  ├─ ParameterDeclaration
// │  │  ├─ ModuleDeclaration
// │  │  ├─ FunctionDefinition
// │  │  └─ StructDefinition
// │  ├─ CompoundStatement
// │  ├─ ExpressionStatement
// │  └─ ControlFlowStatement
// │     ├─ ReturnStatement
// │     ├─ IfStatement
// │     └─ WhileStatement
// └─ Expression
//    ├─ Identifier
//    ├─ IntegerLiteral
//    ├─ StringLiteral
//    ├─ UnaryPrefixExpression
//    ├─ BinaryExpression
//    ├─ MemberAccess
//    ├─ Conditional
//    ├─ FunctionCall
//    └─ Subscript

namespace scatha::ast {

/// MARK: Expressions

/// Abstract node representing any expression.
class SCATHA(API) Expression: public AbstractSyntaxTree {
public:
    using AbstractSyntaxTree::AbstractSyntaxTree;

    /// **Decoration provided by semantic analysis**

    /// Wether the expression refers to a value or a type.
    EntityCategory entityCategory() const {
        expectDecorated();
        return _entityCat;
    }

    /// The value category of this expression.
    ValueCategory valueCategory() const {
        expectDecorated();
        return _valueCat;
    }

    /// The type of the expression. Only valid if: `kind == ::Value`
    sema::TypeID typeID() const {
        expectDecorated();
        return _typeID;
    }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Value`
    bool isValue() const { return entityCategory() == EntityCategory::Value; }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Type`
    bool isType() const { return entityCategory() == EntityCategory::Type; }

    /// Decorate this node.
    void decorate(sema::TypeID typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        _entityCat = entityCat;
        _valueCat  = valueCat;
        _typeID    = typeID;
        markDecorated();
    }

private:
    EntityCategory _entityCat = EntityCategory::Value;
    ValueCategory _valueCat   = ValueCategory::None;
    sema::TypeID _typeID{};
};

/// Concrete node representing an identifier.
class SCATHA(API) Identifier: public Expression {
public:
    explicit Identifier(Token const& token):
        Expression(NodeType::Identifier, token) {}

    /// Literal string value as declared in the source.
    std::string_view value() const { return token().id; }

    /// **Decoration provided by semantic analysis**

    /// ID of the resolved symbol.
    sema::SymbolID symbolID() const {
        expectDecorated();
        return _symbolID;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID,
                  sema::TypeID typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        _symbolID = symbolID;
        Expression::decorate(typeID, valueCat, entityCat);
    }

private:
    sema::SymbolID _symbolID;
};

/// Concrete node representing an integer literal.
class SCATHA(API) IntegerLiteral: public Expression {
public:
    explicit IntegerLiteral(Token const& token):
        Expression(NodeType::IntegerLiteral, token),
        _value(token.toInteger(64)) {}

    APInt value() const { return _value; };

private:
    APInt _value;
};

/// Concrete node representing a boolean literal.
class SCATHA(API) BooleanLiteral: public Expression {
public:
    explicit BooleanLiteral(Token const& token):
        Expression(NodeType::BooleanLiteral, token), _value(token.toBool()) {}

    /// Value as declared in the source code.
    APInt value() const { return _value; }

private:
    APInt _value;
};

/// Concrete node representing a floating point literal.
class SCATHA(API) FloatingPointLiteral: public Expression {
public:
    explicit FloatingPointLiteral(Token const& token):
        Expression(NodeType::FloatingPointLiteral, token),
        _value(token.toFloat()) {}

    APFloat value() const { return _value; }

private:
    APFloat _value;
};

/// Concrete node representing a string literal.
class SCATHA(API) StringLiteral: public Expression {
public:
    explicit StringLiteral(Token const& token):
        Expression(NodeType::StringLiteral, token), _value(token.id) {}

    /// Value as declared in the source code.
    std::string value() const { return _value; }

private:
    std::string _value;
};

/// MARK: Unary Expressions

/// Concrete node representing a unary prefix expression.
class SCATHA(API) UnaryPrefixExpression: public Expression {
public:
    explicit UnaryPrefixExpression(UnaryPrefixOperator op,
                                   UniquePtr<Expression> operand,
                                   Token const& token):
        Expression(NodeType::UnaryPrefixExpression, token),
        operand(std::move(operand)),
        op(op) {}

    /// The operator of this expression.
    UnaryPrefixOperator operation() const { return op; }

    /// The operand of this expression.
    UniquePtr<Expression> operand;

private:
    UnaryPrefixOperator op;
};

/// MARK: Binary Expressions

/// Concrete node representing a binary infix expression.
class SCATHA(API) BinaryExpression: public Expression {
public:
    explicit BinaryExpression(BinaryOperator op,
                              UniquePtr<Expression> lhs,
                              UniquePtr<Expression> rhs,
                              Token const& token):
        Expression(NodeType::BinaryExpression, token),
        lhs(std::move(lhs)),
        rhs(std::move(rhs)),
        op(op) {}

    /// The operator of this expression.
    BinaryOperator operation() const { return op; }

    /// Change the operator of this expression.
    void setOperation(BinaryOperator newOp) { op = newOp; }

    /// The left hand side operand of this expression.
    UniquePtr<Expression> lhs;

    /// The right hand side operand of this expression.
    UniquePtr<Expression> rhs;

private:
    BinaryOperator op;
};

/// Concrete node representing a member access expression.
class SCATHA(API) MemberAccess: public Expression {
public:
    explicit MemberAccess(UniquePtr<Expression> object,
                          UniquePtr<Expression> member,
                          Token const& dotToken):
        Expression(NodeType::MemberAccess, dotToken),
        object(std::move(object)),
        member(std::move(member)) {}

    /// The object being accessed.
    UniquePtr<Expression> object;

    /// The expression to access the object.
    UniquePtr<Expression> member;

    /// **Decoration provided by semantic analysis**

    /// ID of the resolved symbol
    sema::SymbolID symbolID() const {
        expectDecorated();
        return _symbolID;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID,
                  sema::TypeID typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        _symbolID = symbolID;
        Expression::decorate(typeID, valueCat, entityCat);
    }

private:
    sema::SymbolID _symbolID;
};

/// MARK: Ternary Expressions

/// Concrete node representing a conditional expression.
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

    /// The condition to branch on.
    UniquePtr<Expression> condition;

    /// Expression to evaluate if condition is true.
    UniquePtr<Expression> ifExpr;

    /// Expression to evaluate if condition is false.
    UniquePtr<Expression> elseExpr;
};

/// MARK: More Complex Expressions

/// Concrete node representing a function call expression.
class SCATHA(API) FunctionCall: public Expression {
public:
    explicit FunctionCall(UniquePtr<Expression> object, Token const& token):
        Expression(NodeType::FunctionCall, token), object(std::move(object)) {}

    /// The object (function or rather overload set) being called.
    UniquePtr<Expression> object;

    /// List of arguments.
    utl::small_vector<UniquePtr<Expression>> arguments;

    /// **Decoration provided by semantic analysis**

    /// The SymbolID of the resolved function.
    /// Differs from the SymbolID of the object as this ID is resolved by
    /// overload resolution and refers to an actual function while the latter
    /// refers to an overload set or object.
    sema::SymbolID functionID() const {
        expectDecorated();
        return _functionID;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID functionID,
                  sema::TypeID typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        _functionID = functionID;
        Expression::decorate(typeID, valueCat, entityCat);
    }

private:
    sema::SymbolID _functionID;
};

/// Concrete node representing a subscript expression.
class SCATHA(API) Subscript: public Expression {
public:
    explicit Subscript(UniquePtr<Expression> object, Token const& token):
        Expression(NodeType::Subscript, token), object(std::move(object)) {}

    /// The object being indexed.
    UniquePtr<Expression> object;

    /// List of index arguments.
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
    std::string_view name() const {
        return nameIdentifier ? std::string_view(nameIdentifier->token().id) :
                                "";
    }

    /// Identifier expression representing the name of this declaration.
    UniquePtr<Identifier> nameIdentifier;

    /// **Decoration provided by semantic analysis**

    /// SymbolID of this declaration in the symbol table
    sema::SymbolID symbolID() const {
        expectDecorated();
        return _symbolID;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID) {
        _symbolID = symbolID;
        markDecorated();
    }

protected:
    explicit Declaration(NodeType type,
                         Token const& declarator,
                         UniquePtr<Identifier> name):
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
    explicit VariableDeclaration(Token const& declarator,
                                 UniquePtr<Identifier> name):
        Declaration(NodeType::VariableDeclaration,
                    declarator,
                    std::move(name)) {}

    bool isConstant : 1 = false; // Will be set by the parser

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// Expression to initialize this variable. May be null.
    UniquePtr<Expression> initExpression;

    /// **Decoration provided by semantic analysis**

    /// Type of the variable.
    /// Either deduced by the type of initExpression or by declTypename and then
    /// checked against the type of  initExpression
    sema::TypeID typeID() const {
        expectDecorated();
        return _typeID;
    }

    /// Offset of the variable if this is a struct member. Always zero
    /// otherwise.
    size_t offset() const {
        expectDecorated();
        return _offset;
    }

    /// Index of the variable if this is a struct member. Always zero otherwise.
    size_t index() const {
        expectDecorated();
        return _index;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::TypeID typeID) {
        _typeID = typeID;
        Declaration::decorate(symbolID);
    }

    void setOffset(size_t offset) { _offset = offset; }

    void setIndex(size_t index) { _index = index; }

private:
    sema::TypeID _typeID = sema::TypeID::Invalid;
    size_t _offset       = 0;
    size_t _index        = 0;
};

/// Concrete node representing a parameter declaration.
class SCATHA(API) ParameterDeclaration: public Declaration {
public:
    explicit ParameterDeclaration(UniquePtr<Identifier> name,
                                  UniquePtr<Expression> typeExpr):
        Declaration(NodeType::ParameterDeclaration, Token{}, std::move(name)),
        typeExpr(std::move(typeExpr)) {
        if (nameIdentifier) {
            setToken(nameIdentifier->token());
        }
    }

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// **Decoration provided by semantic analysis**

    /// Type of the parameter.
    sema::TypeID typeID() const {
        expectDecorated();
        return _typeID;
    }

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

/// Concrete node representing a compound statement. Declares its own (possibly
/// anonymous) scope.
class SCATHA(API) CompoundStatement: public Statement {
public:
    explicit CompoundStatement(Token const& token):
        Statement(NodeType::CompoundStatement, token) {}

    /// List of statements in the compound statement.
    utl::small_vector<UniquePtr<Statement>> statements;

    /// **Decoration provided by semantic analysis**

    /// Kind of this block scope. Default is 'Anonymous', make sure to set this
    /// to Function or ObjectType in other cases.
    sema::ScopeKind scopeKind() const {
        expectDecorated();
        return _scopeKind;
    }

    /// SymbolID of this declaration in the symbol table
    sema::SymbolID symbolID() const {
        expectDecorated();
        return _symbolID;
    }

    /// Decorate this node.
    void decorate(sema::ScopeKind scopeKind, sema::SymbolID symbolID) {
        _scopeKind = scopeKind;
        _symbolID  = symbolID;
        markDecorated();
    }

private:
    sema::ScopeKind _scopeKind = sema::ScopeKind::Anonymous;
    sema::SymbolID _symbolID;
};

/// Concrete node representing an empty statement (";").
/// Note: This class exists so we don't have to ignore empty statements while
/// parsing but can potentially handle them in some way in semantic analysis.
class SCATHA(API) EmptyStatement: public Statement {
public:
    explicit EmptyStatement(Token const& token):
        Statement(NodeType::EmptyStatement, token) {}
};

/// Concrete node representing the definition of a function.
class SCATHA(API) FunctionDefinition: public Declaration {
public:
    explicit FunctionDefinition(Token const& declarator,
                                UniquePtr<Identifier> name):
        Declaration(NodeType::FunctionDefinition,
                    std::move(declarator),
                    std::move(name)) {}

    /// Typename of the return type as declared in the source code.
    /// Will be null if no return type was declared.
    UniquePtr<Expression> returnTypeExpr;

    /// List of parameter declarations.
    utl::small_vector<UniquePtr<ParameterDeclaration>> parameters;

    /// Body of the function.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// Return type of the function.
    sema::TypeID returnTypeID() const {
        expectDecorated();
        return _returnTypeID;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::TypeID returnTypeID) {
        _returnTypeID = returnTypeID;
        Declaration::decorate(symbolID);
    }

private:
    sema::TypeID _returnTypeID = sema::TypeID::Invalid;
};

/// Concrete node representing the definition of a struct.
class SCATHA(API) StructDefinition: public Declaration {
public:
    explicit StructDefinition(Token const& declarator,
                              UniquePtr<Identifier> name,
                              UniquePtr<CompoundStatement> body):
        Declaration(NodeType::StructDefinition,
                    std::move(declarator),
                    std::move(name)),
        body(std::move(body)) {}

    /// Body of the struct.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID) { Declaration::decorate(symbolID); }
};

/// Concrete node representing a statement that consists of a single expression.
/// May only appear at function scope.
class SCATHA(API) ExpressionStatement: public Statement {
public:
    explicit ExpressionStatement(UniquePtr<Expression> expression):
        Statement(NodeType::ExpressionStatement,
                  expression ? expression->token() : Token{}),
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
    explicit ReturnStatement(Token const& returnToken,
                             UniquePtr<Expression> expression):
        ControlFlowStatement(NodeType::ReturnStatement, returnToken),
        expression(std::move(expression)) {}

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
        thenBlock(std::move(ifBlock)),
        elseBlock(std::move(elseBlock)) {}

    /// Condition to branch on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;

    /// Statement to execute if condition is true.
    UniquePtr<Statement> thenBlock;

    /// Statement to execute if condition is false.
    UniquePtr<Statement> elseBlock;
};

/// Concrete node representing a while statement.
class SCATHA(API) WhileStatement: public ControlFlowStatement {
public:
    explicit WhileStatement(Token const& token,
                            UniquePtr<Expression> condition,
                            UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::WhileStatement, token),
        condition(std::move(condition)),
        block(std::move(block)) {}

    /// Condition to loop on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;

    /// Statement to execute repeatedly.
    UniquePtr<CompoundStatement> block;
};

/// Concrete node representing a do-while statement.
class SCATHA(API) DoWhileStatement: public ControlFlowStatement {
public:
    explicit DoWhileStatement(Token const& token,
                              UniquePtr<Expression> condition,
                              UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::DoWhileStatement, token),
        condition(std::move(condition)),
        block(std::move(block)) {}

    /// Condition to loop on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;

    /// Statement to execute repeatedly.
    UniquePtr<CompoundStatement> block;
};

/// Concrete node representing a for statement.
class SCATHA(API) ForStatement: public ControlFlowStatement {
public:
    explicit ForStatement(Token const& token,
                          UniquePtr<VariableDeclaration> varDecl,
                          UniquePtr<Expression> condition,
                          UniquePtr<Expression> increment,
                          UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::ForStatement, token),
        varDecl(std::move(varDecl)),
        condition(std::move(condition)),
        increment(std::move(increment)),
        block(std::move(block)) {}

    /// Loop variable declared in this statement.
    UniquePtr<VariableDeclaration> varDecl;

    /// Condition to loop on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;

    /// Increment expression
    /// Will be executed after each loop iteration.
    UniquePtr<Expression> increment;

    /// Statement to execute repeatedly.
    UniquePtr<CompoundStatement> block;
};

} // namespace scatha::ast

#endif // SCATHA_AST_AST_H_
