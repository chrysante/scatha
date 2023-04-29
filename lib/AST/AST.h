// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include <scatha/AST/Fwd.h>
#include <scatha/AST/SourceLocation.h>
#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/Sema/Fwd.h>

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

    /// Source range object associated with this node.
    SourceRange sourceRange() const { return _sourceRange; }

    /// Source location object associated with this node.
    SourceLocation sourceLocation() const { return _sourceRange.begin(); }

    /// The parent of this node
    AbstractSyntaxTree* parent() { return _parent; }

    /// \overload
    AbstractSyntaxTree const* parent() const { return _parent; }

protected:
    explicit AbstractSyntaxTree(NodeType type, SourceRange sourceRange):
        _type(type), _sourceRange(sourceRange) {}

    void setSourceRange(SourceRange sourceRange) { _sourceRange = sourceRange; }

    void setParent(AbstractSyntaxTree* parent) { _parent = parent; }

    void setChild(auto&& child) {
        if (child) {
            child->_parent = this;
        }
    }

    void setChildren(auto&& children) {
        for (auto&& child: children) {
            setChild(child);
        }
    }

private:
    friend void scatha::internal::privateDelete(ast::AbstractSyntaxTree*);

private:
    NodeType _type;
    SourceRange _sourceRange;
    AbstractSyntaxTree* _parent = nullptr;
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
    sema::EntityCategory entityCategory() const {
        expectDecorated();
        return _entityCat;
    }

    /// The value category of this expression.
    sema::ValueCategory valueCategory() const {
        expectDecorated();
        return _valueCat;
    }

    /// ID of the resolved symbol (may be `Invalid`)
    sema::Entity* entity() {
        return const_cast<sema::Entity*>(std::as_const(*this).entity());
    }

    /// \overload
    sema::Entity const* entity() const {
        expectDecorated();
        return _entity;
    }

    /// The type of the expression. Only valid if: `kind == ::Value`
    sema::QualType const* type() const {
        expectDecorated();
        return _type;
    }

    /// The type of the expression, if this is a value. If this is a
    /// type, returns that type
    sema::QualType const* typeOrTypeEntity() const;

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Value`
    bool isValue() const {
        return entityCategory() == sema::EntityCategory::Value;
    }

    /// Convenience wrapper
    bool isLValue() const {
        return isValue() && valueCategory() == sema::ValueCategory::LValue;
    }

    /// Convenience wrapper
    bool isRValue() const {
        return isValue() && valueCategory() == sema::ValueCategory::RValue;
    }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Type`
    bool isType() const {
        return entityCategory() == sema::EntityCategory::Type;
    }

    /// Decorate this node.
    void decorate(sema::Entity* entity,
                  sema::QualType const* type                    = nullptr,
                  std::optional<sema::ValueCategory> valueCat   = std::nullopt,
                  std::optional<sema::EntityCategory> entityCat = std::nullopt);

private:
    sema::Entity* _entity = nullptr;
    sema::QualType const* _type{};
    sema::ValueCategory _valueCat   = sema::ValueCategory::None;
    sema::EntityCategory _entityCat = sema::EntityCategory::Indeterminate;
};

/// Concrete node representing an identifier.
class SCATHA_API Identifier: public Expression {
public:
    explicit Identifier(SourceRange sourceRange, std::string id):
        Expression(NodeType::Identifier, sourceRange), _value(id) {}

    /// Literal string value as declared in the source.
    std::string const& value() const { return _value; }

private:
    std::string _value;
};

/// Concrete node representing an integer literal.
class SCATHA_API IntegerLiteral: public Expression {
public:
    explicit IntegerLiteral(SourceRange sourceRange, APInt value):
        Expression(NodeType::IntegerLiteral, sourceRange),
        _value(std::move(value)) {}

    APInt value() const { return _value; };

private:
    APInt _value;
};

/// Concrete node representing a boolean literal.
class SCATHA_API BooleanLiteral: public Expression {
public:
    explicit BooleanLiteral(SourceRange sourceRange, APInt value):
        Expression(NodeType::BooleanLiteral, sourceRange),
        _value(std::move(value)) {}

    /// Value as declared in the source code.
    APInt value() const { return _value; }

private:
    APInt _value;
};

/// Concrete node representing a floating point literal.
class SCATHA_API FloatingPointLiteral: public Expression {
public:
    explicit FloatingPointLiteral(SourceRange sourceRange, APFloat value):
        Expression(NodeType::FloatingPointLiteral, sourceRange),
        _value(std::move(value)) {}

    APFloat value() const { return _value; }

private:
    APFloat _value;
};

/// Concrete node representing a string literal.
class SCATHA_API StringLiteral: public Expression {
public:
    explicit StringLiteral(SourceRange sourceRange, std::string value):
        Expression(NodeType::StringLiteral, sourceRange),
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
                                   SourceRange sourceRange):
        Expression(NodeType::UnaryPrefixExpression, sourceRange),
        operand(std::move(operand)),
        op(op) {
        setChild(this->operand.get());
    }

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
                              SourceRange sourceRange):
        Expression(NodeType::BinaryExpression, sourceRange),
        lhs(std::move(lhs)),
        rhs(std::move(rhs)),
        op(op) {
        setChild(this->lhs.get());
        setChild(this->rhs.get());
    }

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
                          UniquePtr<Identifier> member,
                          SourceRange sourceRange):
        Expression(NodeType::MemberAccess, sourceRange),
        object(std::move(object)),
        member(std::move(member)) {
        setChild(this->object.get());
        setChild(this->member.get());
    }

    /// The object being accessed.
    UniquePtr<Expression> object;

    /// The expression to access the object.
    UniquePtr<Identifier> member;
};

/// Concrete node representing a reference expression.
class SCATHA_API ReferenceExpression: public Expression {
public:
    explicit ReferenceExpression(UniquePtr<Expression> referred,
                                 SourceRange sourceRange):
        Expression(NodeType::ReferenceExpression, sourceRange),
        referred(std::move(referred)) {}

    /// The object being referred to.
    UniquePtr<Expression> referred;
};

/// Concrete node representing a `unique` expression.
class SCATHA_API UniqueExpression: public Expression {
public:
    explicit UniqueExpression(UniquePtr<Expression> initExpr,
                              SourceRange sourceRange):
        Expression(NodeType::UniqueExpression, sourceRange),
        initExpr(std::move(initExpr)) {}

    /// The initializing expression
    UniquePtr<Expression> initExpr;

    /// Decorate this node.
    void decorate(sema::QualType const* type) {
        Expression::decorate(nullptr, type);
    }
};

/// MARK: Ternary Expressions

/// Concrete node representing a conditional expression.
class SCATHA_API Conditional: public Expression {
public:
    explicit Conditional(UniquePtr<Expression> condition,
                         UniquePtr<Expression> ifExpr,
                         UniquePtr<Expression> elseExpr,
                         SourceRange sourceRange):
        Expression(NodeType::Conditional, sourceRange),
        condition(std::move(condition)),
        ifExpr(std::move(ifExpr)),
        elseExpr(std::move(elseExpr)) {
        setChild(this->condition.get());
        setChild(this->ifExpr.get());
        setChild(this->elseExpr.get());
    }

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
                          utl::small_vector<UniquePtr<Expression>> arguments,
                          SourceRange sourceRange):
        Expression(NodeType::FunctionCall, sourceRange),
        object(std::move(object)),
        arguments(std::move(arguments)) {
        setChild(this->object.get());
        setChildren(this->arguments);
    }

    /// The object (function or rather overload set) being called.
    UniquePtr<Expression> object;

    /// List of arguments.
    utl::small_vector<UniquePtr<Expression>> arguments;

    /// **Decoration provided by semantic analysis**

    /// The  resolved function.
    /// Differs from object because in that the object refers to  the overload
    /// set
    template <typename F = sema::Function>
    F* function() {
        return const_cast<F*>(std::as_const(*this).function());
    }

    /// \overload
    template <typename F = sema::Function>
    F const* function() const {
        return cast<F const*>(entity());
    }
};

/// Concrete node representing a subscript expression.
class SCATHA_API Subscript: public Expression {
public:
    explicit Subscript(UniquePtr<Expression> object,
                       utl::small_vector<UniquePtr<Expression>> arguments,
                       SourceRange sourceRange):
        Expression(NodeType::Subscript, sourceRange),
        object(std::move(object)),
        arguments(std::move(arguments)) {
        setChild(this->object.get());
        setChildren(this->arguments);
    }

    /// The object being indexed.
    UniquePtr<Expression> object;

    /// List of index arguments.
    utl::small_vector<UniquePtr<Expression>> arguments;
};

/// Represents a list expression, i.e. `[a, b, c]`
class SCATHA_API ListExpression: public Expression {
    static auto impl(auto* self) {
        return ranges::views::transform(self->elems,
                                        [](auto& p) { return p.get(); });
    }

public:
    ListExpression(utl::small_vector<UniquePtr<Expression>> elems,
                   SourceRange sourceRange):
        Expression(NodeType::ListExpression, sourceRange),
        elems(std::move(elems)) {
        setChildren(this->elems);
    }

    size_t size() const { return elems.size(); }

    bool empty() const { return elems.empty(); }

    auto begin() { return impl(this).begin(); }
    auto begin() const { return impl(this).begin(); }

    auto end() { return impl(this).end(); }
    auto end() const { return impl(this).end(); }

    Expression* front() { return impl(this).front(); }
    Expression const* front() const { return impl(this).front(); }

    Expression* back() { return impl(this).back(); }
    Expression const* back() const { return impl(this).back(); }

    Expression* operator[](size_t index) { return elems[index].get(); }
    Expression const* operator[](size_t index) const {
        return elems[index].get();
    }

private:
    utl::small_vector<UniquePtr<Expression>> elems;
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

    /// Entity this declaration corresponds to
    sema::Entity* entity() {
        return const_cast<sema::Entity*>(std::as_const(*this).entity());
    }

    /// \overload
    sema::Entity const* entity() const {
        expectDecorated();
        return _entity;
    }

    /// Decorate this node.
    void decorate(sema::Entity* entity) {
        _entity = entity;
        markDecorated();
    }

protected:
    explicit Declaration(NodeType type,
                         SourceRange sourceRange,
                         UniquePtr<Identifier> name):
        Statement(type, sourceRange), nameIdentifier(std::move(name)) {}

private:
    sema::Entity* _entity = nullptr;
};

/// Concrete node representing a translation unit.
class SCATHA_API TranslationUnit: public AbstractSyntaxTree {
public:
    TranslationUnit():
        AbstractSyntaxTree(NodeType::TranslationUnit, SourceRange{}) {}

    /// List of declarations in the translation unit.
    utl::small_vector<UniquePtr<Declaration>> declarations;
};

/// Concrete node representing a variable declaration.
class SCATHA_API VariableDeclaration: public Declaration {
public:
    explicit VariableDeclaration(SourceRange sourceRange,
                                 UniquePtr<Identifier> name):
        Declaration(NodeType::VariableDeclaration,
                    sourceRange,
                    std::move(name)) {}

    bool isConstant : 1 = false; // Will be set by the parser

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// Expression to initialize this variable. May be null.
    UniquePtr<Expression> initExpression;

    /// **Decoration provided by semantic analysis**

    /// Declared variable
    template <typename V = sema::Variable>
    V* variable() {
        return const_cast<V*>(std::as_const(*this).variable());
    }

    /// \overload
    template <typename V = sema::Variable>
    V const* variable() const {
        return cast<V const*>(entity());
    }

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
    void decorate(sema::Entity* entity, sema::QualType const* type) {
        _type = type;
        Declaration::decorate(entity);
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
        ParameterDeclaration(NodeType::ParameterDeclaration,
                             SourceRange{},
                             std::move(name),
                             std::move(typeExpr)) {}

    /// Typename declared in the source code. Null if no typename was declared.
    UniquePtr<Expression> typeExpr;

    /// **Decoration provided by semantic analysis**

    /// Type of the parameter.
    sema::QualType const* type() const {
        expectDecorated();
        return _type;
    }

    /// Decorate this node.
    void decorate(sema::Entity* entity, sema::QualType const* type) {
        _type = type;
        Declaration::decorate(entity);
    }

protected:
    explicit ParameterDeclaration(NodeType nodeType,
                                  SourceRange sourceRange,
                                  UniquePtr<Identifier> name,
                                  UniquePtr<Expression> typeExpr):
        Declaration(nodeType, sourceRange, std::move(name)),
        typeExpr(std::move(typeExpr)) {
        setChild(this->typeExpr.get());
        if (nameIdentifier) {
            setSourceRange(nameIdentifier->sourceRange());
        }
    }

private:
    sema::QualType const* _type = nullptr;
};

class ThisParameter: public ParameterDeclaration {
public:
    explicit ThisParameter(SourceRange sourceRange,
                           sema::TypeQualifiers qualifiers):
        ParameterDeclaration(NodeType::ThisParameter,
                             sourceRange,
                             nullptr,
                             nullptr),
        quals(qualifiers) {}

    sema::TypeQualifiers qualifiers() const { return quals; }

private:
    sema::TypeQualifiers quals;
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
    explicit CompoundStatement(
        SourceRange sourceRange,
        utl::small_vector<UniquePtr<Statement>> statements):
        Statement(NodeType::CompoundStatement, sourceRange),
        statements(std::move(statements)) {
        setChildren(this->statements);
    }

    /// List of statements in the compound statement.
    utl::small_vector<UniquePtr<Statement>> statements;

    /// **Decoration provided by semantic analysis**

    /// Corresponding scope in symbol table
    sema::Scope* scope() {
        return const_cast<sema::Scope*>(std::as_const(*this).scope());
    }

    /// \overload
    sema::Scope const* scope() const {
        expectDecorated();
        return _scope;
    }

    /// Decorate this node.
    void decorate(sema::Scope* scope) {
        _scope = scope;
        markDecorated();
    }

private:
    sema::Scope* _scope = nullptr;
};

/// Concrete node representing an empty statement (";").
/// Note: This class exists so we don't have to ignore empty statements while
/// parsing but can potentially handle them in some way in semantic analysis.
class SCATHA_API EmptyStatement: public Statement {
public:
    explicit EmptyStatement(SourceRange sourceRange):
        Statement(NodeType::EmptyStatement, sourceRange) {}
};

/// Concrete node representing the definition of a function.
class SCATHA_API FunctionDefinition: public Declaration {
public:
    explicit FunctionDefinition(SourceRange sourceRange,
                                UniquePtr<Identifier> name):
        Declaration(NodeType::FunctionDefinition,
                    sourceRange,
                    std::move(name)) {}

    /// Typename of the return type as declared in the source code.
    /// Will be `nullptr` if no return type was declared.
    UniquePtr<Expression> returnTypeExpr;

    /// List of parameter declarations.
    utl::small_vector<UniquePtr<ParameterDeclaration>> parameters;

    /// Body of the function.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// The function being defined
    template <typename F = sema::Function>
    F* function() {
        return const_cast<F*>(std::as_const(*this).function());
    }

    /// \overload
    template <typename F = sema::Function>
    F const* function() const {
        return cast<F const*>(entity());
    }

    /// Return type of the function.
    sema::QualType const* returnType() const {
        expectDecorated();
        return _returnType;
    }

    /// Decorate this node.
    void decorate(sema::Entity* entity, sema::QualType const* returnType) {
        _returnType = returnType;
        Declaration::decorate(entity);
    }

private:
    sema::QualType const* _returnType = nullptr;
};

/// Concrete node representing the definition of a struct.
class SCATHA_API StructDefinition: public Declaration {
public:
    explicit StructDefinition(SourceRange sourceRange,
                              UniquePtr<Identifier> name,
                              UniquePtr<CompoundStatement> body):
        Declaration(NodeType::StructDefinition, sourceRange, std::move(name)),
        body(std::move(body)) {
        setChild(this->body.get());
    }

    /// Body of the struct.
    UniquePtr<CompoundStatement> body;

    /// **Decoration provided by semantic analysis**

    /// Decorate this node.
    void decorate(sema::Entity* entity) { Declaration::decorate(entity); }
};

/// Concrete node representing a statement that consists of a single expression.
/// May only appear at function scope.
class SCATHA_API ExpressionStatement: public Statement {
public:
    explicit ExpressionStatement(UniquePtr<Expression> expression):
        Statement(NodeType::ExpressionStatement,
                  expression ? expression->sourceRange() : SourceRange{}),
        expression(std::move(expression)) {
        setChild(this->expression.get());
    }

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
    explicit ReturnStatement(SourceRange sourceRange,
                             UniquePtr<Expression> expression):
        ControlFlowStatement(NodeType::ReturnStatement, sourceRange),
        expression(std::move(expression)) {
        setChild(this->expression.get());
    }

    /// The returned expression. May be null in case of a void function.
    UniquePtr<Expression> expression;
};

/// Concrete node representing an if/else statement.
class SCATHA_API IfStatement: public ControlFlowStatement {
public:
    explicit IfStatement(SourceRange sourceRange,
                         UniquePtr<Expression> condition,
                         UniquePtr<Statement> ifBlock,
                         UniquePtr<Statement> elseBlock):
        ControlFlowStatement(NodeType::IfStatement, sourceRange),
        condition(std::move(condition)),
        thenBlock(std::move(ifBlock)),
        elseBlock(std::move(elseBlock)) {
        setChild(this->condition.get());
        setChild(this->thenBlock.get());
        setChild(this->elseBlock.get());
    }

    /// Condition to branch on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    UniquePtr<Expression> condition;

    /// Statement to execute if condition is true.
    UniquePtr<Statement> thenBlock;

    /// Statement to execute if condition is false.
    UniquePtr<Statement> elseBlock;
};

/// Represents a `for`, `while` or `do`/`while` loop.
class SCATHA_API LoopStatement: public ControlFlowStatement {
public:
    explicit LoopStatement(SourceRange sourceRange,
                           LoopKind kind,
                           UniquePtr<VariableDeclaration> varDecl,
                           UniquePtr<Expression> condition,
                           UniquePtr<Expression> increment,
                           UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::LoopStatement, sourceRange),
        varDecl(std::move(varDecl)),
        condition(std::move(condition)),
        increment(std::move(increment)),
        block(std::move(block)),
        _kind(kind) {
        setChild(this->varDecl.get());
        setChild(this->condition.get());
        setChild(this->increment.get());
        setChild(this->block.get());
    }

    /// Loop variable declared in this statement.
    /// Only non-null if `kind() == For`
    UniquePtr<VariableDeclaration> varDecl;

    /// Condition to loop on.
    UniquePtr<Expression> condition;

    /// Increment expression
    /// Only non-null if `kind() == For`
    UniquePtr<Expression> increment;

    /// Statement to execute repeatedly.
    UniquePtr<CompoundStatement> block;

    /// Either `while` or `do`/`while`
    LoopKind kind() const { return _kind; }

private:
    LoopKind _kind;
};

/// Represents a `break` or `continue` statement.
class SCATHA_API JumpStatement: public ControlFlowStatement {
public:
    enum Kind { Break, Continue };

    explicit JumpStatement(Kind kind, SourceRange sourceRange):
        ControlFlowStatement(NodeType::JumpStatement, sourceRange),
        _kind(kind) {}

    Kind kind() const { return _kind; }

private:
    Kind _kind;
};

} // namespace scatha::ast

#endif // SCATHA_AST_AST_H_
