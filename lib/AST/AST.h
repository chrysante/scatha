// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <string>

#include <utl/vector.hpp>

#include <scatha/AST/Fwd.h>
#include <scatha/AST/SourceLocation.h>
#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/UniquePtr.h>
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

namespace internal {

class Decoratable {
public:
    bool isDecorated() const { return decorated; }

protected:
    void expectDecorated() const {
        SC_EXPECT(isDecorated(), "Requested decoration on undecorated node.");
    }
    void markDecorated() { decorated = true; }

private:
    bool decorated = false;
};

} // namespace internal

/// ## Base class for all nodes in the AST
/// Every derived class must specify its runtime type in the constructor via the
/// `NodeType` enum.
class SCATHA_API AbstractSyntaxTree: public internal::Decoratable {
public:
    /// Runtime type of this node
    NodeType nodeType() const { return _type; }

    /// Source location object associated with this node.
    SourceLocation sourceLocation() const { return _sourceLoc; }

protected:
    explicit AbstractSyntaxTree(NodeType type, SourceLocation sourceLoc):
        _type(type), _sourceLoc(sourceLoc) {}

    void setSourceLocation(SourceLocation sourceLoc) { _sourceLoc = sourceLoc; }

private:
    friend void scatha::internal::privateDelete(ast::AbstractSyntaxTree*);

private:
    NodeType _type;
    SourceLocation _sourceLoc;
};

// For `dyncast` compatibilty
NodeType dyncast_get_type(
    std::derived_from<AbstractSyntaxTree> auto const& node) {
    return node.nodeType();
}

/// MARK: Expressions

/// Abstract node representing any expression.
class SCATHA_API Expression: public AbstractSyntaxTree {
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

    /// ID of the resolved symbol (may be `Invalid`)
    sema::SymbolID symbolID() const {
        expectDecorated();
        return _symbolID;
    }

    /// The type of the expression. Only valid if: `kind == ::Value`
    sema::QualType const* type() const {
        expectDecorated();
        return _type;
    }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Value`
    bool isValue() const { return entityCategory() == EntityCategory::Value; }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Type`
    bool isType() const { return entityCategory() == EntityCategory::Type; }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID,
                  sema::QualType const* type,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        _entityCat = entityCat;
        _valueCat  = valueCat;
        _symbolID  = symbolID;
        _type      = type;
        markDecorated();
    }

private:
    EntityCategory _entityCat = EntityCategory::Value;
    ValueCategory _valueCat   = ValueCategory::None;
    sema::SymbolID _symbolID  = sema::SymbolID::Invalid;
    sema::QualType const* _type{};
};

/// Concrete node representing an identifier.
class SCATHA_API Identifier: public Expression {
public:
    explicit Identifier(SourceLocation sourceLoc, std::string id):
        Expression(NodeType::Identifier, sourceLoc), _value(id) {}

    /// Literal string value as declared in the source.
    std::string const& value() const { return _value; }

    /// **Decoration provided by semantic analysis**

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID,
                  sema::QualType const* typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        Expression::decorate(symbolID, typeID, valueCat, entityCat);
    }

private:
    std::string _value;
};

/// Concrete node representing an integer literal.
class SCATHA_API IntegerLiteral: public Expression {
public:
    explicit IntegerLiteral(SourceLocation sourceLoc, APInt value):
        Expression(NodeType::IntegerLiteral, sourceLoc),
        _value(std::move(value)) {}

    APInt value() const { return _value; };

private:
    APInt _value;
};

/// Concrete node representing a boolean literal.
class SCATHA_API BooleanLiteral: public Expression {
public:
    explicit BooleanLiteral(SourceLocation sourceLoc, APInt value):
        Expression(NodeType::BooleanLiteral, sourceLoc),
        _value(std::move(value)) {}

    /// Value as declared in the source code.
    APInt value() const { return _value; }

private:
    APInt _value;
};

/// Concrete node representing a floating point literal.
class SCATHA_API FloatingPointLiteral: public Expression {
public:
    explicit FloatingPointLiteral(SourceLocation sourceLoc, APFloat value):
        Expression(NodeType::FloatingPointLiteral, sourceLoc),
        _value(std::move(value)) {}

    APFloat value() const { return _value; }

private:
    APFloat _value;
};

/// Concrete node representing a string literal.
class SCATHA_API StringLiteral: public Expression {
public:
    explicit StringLiteral(SourceLocation sourceLoc, std::string value):
        Expression(NodeType::StringLiteral, sourceLoc),
        _value(std::move(value)) {}

    /// Value as declared in the source code.
    std::string value() const { return _value; }

private:
    std::string _value;
};

/// MARK: Unary Expressions

/// Concrete node representing a unary prefix expression.
class SCATHA_API UnaryPrefixExpression: public Expression {
public:
    explicit UnaryPrefixExpression(UnaryPrefixOperator op,
                                   UniquePtr<Expression> operand,
                                   SourceLocation sourceLoc):
        Expression(NodeType::UnaryPrefixExpression, sourceLoc),
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
class SCATHA_API BinaryExpression: public Expression {
public:
    explicit BinaryExpression(BinaryOperator op,
                              UniquePtr<Expression> lhs,
                              UniquePtr<Expression> rhs,
                              SourceLocation sourceLoc):
        Expression(NodeType::BinaryExpression, sourceLoc),
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
class SCATHA_API MemberAccess: public Expression {
public:
    explicit MemberAccess(UniquePtr<Expression> object,
                          UniquePtr<Expression> member,
                          SourceLocation sourceLoc):
        Expression(NodeType::MemberAccess, sourceLoc),
        object(std::move(object)),
        member(std::move(member)) {}

    /// The object being accessed.
    UniquePtr<Expression> object;

    /// The expression to access the object.
    UniquePtr<Expression> member;

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID,
                  sema::QualType const* typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        Expression::decorate(symbolID, typeID, valueCat, entityCat);
    }
};

/// Concrete node representing a reference expression.
class SCATHA_API ReferenceExpression: public Expression {
public:
    explicit ReferenceExpression(UniquePtr<Expression> referred,
                                 SourceLocation sourceLoc):
        Expression(NodeType::ReferenceExpression, sourceLoc),
        referred(std::move(referred)) {}

    /// The object being referred to.
    UniquePtr<Expression> referred;

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID,
                  sema::QualType const* typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        Expression::decorate(symbolID, typeID, valueCat, entityCat);
    }
};

/// MARK: Ternary Expressions

/// Concrete node representing a conditional expression.
class SCATHA_API Conditional: public Expression {
public:
    explicit Conditional(UniquePtr<Expression> condition,
                         UniquePtr<Expression> ifExpr,
                         UniquePtr<Expression> elseExpr,
                         SourceLocation sourceLoc):
        Expression(NodeType::Conditional, sourceLoc),
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
class SCATHA_API FunctionCall: public Expression {
public:
    explicit FunctionCall(UniquePtr<Expression> object,
                          SourceLocation sourceLoc):
        Expression(NodeType::FunctionCall, sourceLoc),
        object(std::move(object)) {}

    /// The object (function or rather overload set) being called.
    UniquePtr<Expression> object;

    /// List of arguments.
    utl::small_vector<UniquePtr<Expression>> arguments;

    /// **Decoration provided by semantic analysis**

    /// The SymbolID of the resolved function.
    /// Differs from the SymbolID of the object as this ID is resolved by
    /// overload resolution and refers to an actual function while the latter
    /// refers to an overload set or object.
    sema::SymbolID functionID() const { return symbolID(); }

    /// Decorate this node.
    void decorate(sema::SymbolID functionID,
                  sema::QualType const* typeID,
                  ValueCategory valueCat,
                  EntityCategory entityCat = EntityCategory::Value) {
        Expression::decorate(functionID, typeID, valueCat, entityCat);
    }
};

/// Concrete node representing a subscript expression.
class SCATHA_API Subscript: public Expression {
public:
    explicit Subscript(UniquePtr<Expression> object, SourceLocation sourceLoc):
        Expression(NodeType::Subscript, sourceLoc), object(std::move(object)) {}

    /// The object being indexed.
    UniquePtr<Expression> object;

    /// List of index arguments.
    utl::small_vector<UniquePtr<Expression>> arguments;
};

/// Abstract node representing a statement.
class SCATHA_API Statement: public AbstractSyntaxTree {
public:
    using AbstractSyntaxTree::AbstractSyntaxTree;
};

/// Abstract node representing a declaration.
class SCATHA_API Declaration: public Statement {
public:
    /// Name of the declared symbol as written in the source code.
    std::string_view name() const {
        return nameIdentifier ? nameIdentifier->value() : std::string_view{};
    }

    /// Identifier expression representing the name of this declaration.
    UniquePtr<Identifier> nameIdentifier;

    /// Access specifier. `None` if none was specified.
    AccessSpec accessSpec = AccessSpec::None;

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
                         SourceLocation sourceLoc,
                         UniquePtr<Identifier> name):
        Statement(type, sourceLoc), nameIdentifier(std::move(name)) {}

private:
    sema::SymbolID _symbolID;
};

/// Concrete node representing a translation unit.
class SCATHA_API TranslationUnit: public AbstractSyntaxTree {
public:
    TranslationUnit():
        AbstractSyntaxTree(NodeType::TranslationUnit, SourceLocation{}) {}

    /// List of declarations in the translation unit.
    utl::small_vector<UniquePtr<Declaration>> declarations;
};

/// Concrete node representing a variable declaration.
class SCATHA_API VariableDeclaration: public Declaration {
public:
    explicit VariableDeclaration(SourceLocation sourceLoc,
                                 UniquePtr<Identifier> name):
        Declaration(NodeType::VariableDeclaration, sourceLoc, std::move(name)) {
    }

    bool isConstant : 1 = false; // Will be set by the parser

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// Expression to initialize this variable. May be null.
    UniquePtr<Expression> initExpression;

    /// **Decoration provided by semantic analysis**

    /// Type of the variable.
    /// Either deduced by the type of initExpression or by declTypename and then
    /// checked against the type of  initExpression
    sema::QualType const* type() const {
        expectDecorated();
        return _type;
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
    void decorate(sema::SymbolID symbolID, sema::QualType const* type) {
        _type = type;
        Declaration::decorate(symbolID);
    }

    void setOffset(size_t offset) { _offset = offset; }

    void setIndex(size_t index) { _index = index; }

private:
    sema::QualType const* _type = nullptr;
    size_t _offset              = 0;
    size_t _index               = 0;
};

/// Concrete node representing a parameter declaration.
class SCATHA_API ParameterDeclaration: public Declaration {
public:
    explicit ParameterDeclaration(UniquePtr<Identifier> name,
                                  UniquePtr<Expression> typeExpr):
        Declaration(NodeType::ParameterDeclaration,
                    SourceLocation{},
                    std::move(name)),
        typeExpr(std::move(typeExpr)) {
        if (nameIdentifier) {
            setSourceLocation(nameIdentifier->sourceLocation());
        }
    }

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// **Decoration provided by semantic analysis**

    /// Type of the parameter.
    sema::QualType const* type() const {
        expectDecorated();
        return _type;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::QualType const* type) {
        _type = type;
        Declaration::decorate(symbolID);
    }

private:
    sema::QualType const* _type = nullptr;
};

/// Nothing to see here yet...
class SCATHA_API ModuleDeclaration: public Declaration {
public:
    ModuleDeclaration() = delete;
};

/// Concrete node representing a compound statement. Declares its own (possibly
/// anonymous) scope.
class SCATHA_API CompoundStatement: public Statement {
public:
    explicit CompoundStatement(SourceLocation sourceLoc):
        Statement(NodeType::CompoundStatement, sourceLoc) {}

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
class SCATHA_API EmptyStatement: public Statement {
public:
    explicit EmptyStatement(SourceLocation sourceLoc):
        Statement(NodeType::EmptyStatement, sourceLoc) {}
};

/// Concrete node representing the definition of a function.
class SCATHA_API FunctionDefinition: public Declaration {
public:
    explicit FunctionDefinition(SourceLocation sourceLoc,
                                UniquePtr<Identifier> name):
        Declaration(NodeType::FunctionDefinition, sourceLoc, std::move(name)) {}

    /// Typename of the return type as declared in the source code.
    /// Will be null if no return type was declared.
    UniquePtr<Expression> returnTypeExpr;

    /// List of parameter declarations.
    utl::small_vector<UniquePtr<ParameterDeclaration>> parameters;

    /// Body of the function.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// Return type of the function.
    sema::QualType const* returnType() const {
        expectDecorated();
        return _returnType;
    }

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID, sema::QualType const* returnType) {
        _returnType = returnType;
        Declaration::decorate(symbolID);
    }

private:
    sema::QualType const* _returnType = nullptr;
};

/// Concrete node representing the definition of a struct.
class SCATHA_API StructDefinition: public Declaration {
public:
    explicit StructDefinition(SourceLocation sourceLoc,
                              UniquePtr<Identifier> name,
                              UniquePtr<CompoundStatement> body):
        Declaration(NodeType::StructDefinition, sourceLoc, std::move(name)),
        body(std::move(body)) {}

    /// Body of the struct.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// Decorate this node.
    void decorate(sema::SymbolID symbolID) { Declaration::decorate(symbolID); }
};

/// Concrete node representing a statement that consists of a single expression.
/// May only appear at function scope.
class SCATHA_API ExpressionStatement: public Statement {
public:
    explicit ExpressionStatement(UniquePtr<Expression> expression):
        Statement(NodeType::ExpressionStatement,
                  expression ? expression->sourceLocation() : SourceLocation{}),
        expression(std::move(expression)) {}

    /// The expression
    UniquePtr<Expression> expression;
};

/// Abstract node representing any control flow statement like if, while, for,
/// return etc. May only appear at function scope.
class SCATHA_API ControlFlowStatement: public Statement {
protected:
    using Statement::Statement;
};

/// Concrete node representing a return statement.
class SCATHA_API ReturnStatement: public ControlFlowStatement {
public:
    explicit ReturnStatement(SourceLocation sourceLoc,
                             UniquePtr<Expression> expression):
        ControlFlowStatement(NodeType::ReturnStatement, sourceLoc),
        expression(std::move(expression)) {}

    /// The returned expression. May be null in case of a void function.
    UniquePtr<Expression> expression;
};

/// Concrete node representing an if/else statement.
class SCATHA_API IfStatement: public ControlFlowStatement {
public:
    explicit IfStatement(SourceLocation sourceLoc,
                         UniquePtr<Expression> condition,
                         UniquePtr<Statement> ifBlock,
                         UniquePtr<Statement> elseBlock):
        ControlFlowStatement(NodeType::IfStatement, sourceLoc),
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
class SCATHA_API WhileStatement: public ControlFlowStatement {
public:
    explicit WhileStatement(SourceLocation sourceLoc,
                            UniquePtr<Expression> condition,
                            UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::WhileStatement, sourceLoc),
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
class SCATHA_API DoWhileStatement: public ControlFlowStatement {
public:
    explicit DoWhileStatement(SourceLocation sourceLoc,
                              UniquePtr<Expression> condition,
                              UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::DoWhileStatement, sourceLoc),
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
class SCATHA_API ForStatement: public ControlFlowStatement {
public:
    explicit ForStatement(SourceLocation sourceLoc,
                          UniquePtr<VariableDeclaration> varDecl,
                          UniquePtr<Expression> condition,
                          UniquePtr<Expression> increment,
                          UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::ForStatement, sourceLoc),
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

/// Represents a `break;` statement.
class SCATHA_API BreakStatement: public ControlFlowStatement {
public:
    explicit BreakStatement(SourceLocation sourceLoc):
        ControlFlowStatement(NodeType::BreakStatement, sourceLoc) {}
};

/// Represents a `continue;` statement.
class SCATHA_API ContinueStatement: public ControlFlowStatement {
public:
    explicit ContinueStatement(SourceLocation sourceLoc):
        ControlFlowStatement(NodeType::ContinueStatement, sourceLoc) {}
};

} // namespace scatha::ast

#endif // SCATHA_AST_AST_H_
