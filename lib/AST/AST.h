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
// │  │  │  └─ ThisParameter
// │  │  ├─ ModuleDeclaration
// │  │  ├─ FunctionDefinition
// │  │  └─ StructDefinition
// │  ├─ CompoundStatement
// │  ├─ ExpressionStatement
// │  └─ ControlFlowStatement
// │     ├─ ReturnStatement
// │     ├─ IfStatement
// │     └─ LoopStatement
// └─ Expression
//    ├─ Identifier
//    ├─ Literal
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
    template <typename AST>
    static constexpr auto transform = ranges::views::transform(
        [](auto& p) -> AST* { return cast_or_null<AST*>(p.get()); });

    template <typename AST>
    auto getChildren() const {
        return _children | transform<AST>;
    }

protected:
    template <typename AST>
    auto dropChildren(size_t count) const {
        return _children | ranges::views::drop(count) | transform<AST>;
    }

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

    /// The children of this node
    template <typename AST = AbstractSyntaxTree>
    auto children() {
        return getChildren<AST>();
    }

    /// \overload
    template <typename AST = AbstractSyntaxTree>
    auto children() const {
        return getChildren<AST const>();
    }

    /// The child at index \p index
    template <typename AST = AbstractSyntaxTree>
    AST* child(size_t index) {
        return cast_or_null<AST*>(children()[utl::narrow_cast<ssize_t>(index)]);
    }

    /// \overload
    template <typename AST = AbstractSyntaxTree>
    AST const* child(size_t index) const {
        return cast_or_null<AST const*>(
            children()[utl::narrow_cast<ssize_t>(index)]);
    }

protected:
    explicit AbstractSyntaxTree(NodeType type,
                                SourceRange sourceRange,
                                auto&&... children):
        _type(type), _sourceRange(sourceRange) {
        (addChildren(std::move(children)), ...);
    }

    void setSourceRange(SourceRange sourceRange) { _sourceRange = sourceRange; }

private:
    template <typename T>
    void addChildren(T&& child) {
        if constexpr (ranges::range<T>) {
            for (auto&& c: child) {
                addChildren(std::move(c));
            }
        }
        else {
            if (child) {
                child->_parent = this;
            }
            _children.push_back(std::move(child));
        }
    }

    friend void scatha::internal::privateDelete(ast::AbstractSyntaxTree*);

private:
    NodeType _type;
    SourceRange _sourceRange;
    AbstractSyntaxTree* _parent = nullptr;
    utl::small_vector<UniquePtr<AbstractSyntaxTree>> _children;
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

/// Concrete node representing a literal.
class SCATHA_API Literal: public Expression {
public:
    using ValueType = std::variant<APInt, APInt, APFloat, void*, std::string>;

    explicit Literal(SourceRange sourceRange,
                     LiteralKind kind,
                     ValueType value):
        Expression(NodeType::Literal, sourceRange),
        _kind(kind),
        _value(std::move(value)) {}

    LiteralKind kind() const { return _kind; }

    ValueType value() const { return _value; };

    template <LiteralKind Kind>
    auto value() const {
        return std::get<static_cast<size_t>(Kind)>(_value);
    };

private:
    LiteralKind _kind;
    ValueType _value;
};

/// MARK: Unary Expressions

/// Concrete node representing a unary prefix expression.
class SCATHA_API UnaryPrefixExpression: public Expression {
public:
    explicit UnaryPrefixExpression(UnaryPrefixOperator op,
                                   UniquePtr<Expression> operand,
                                   SourceRange sourceRange):
        Expression(NodeType::UnaryPrefixExpression,
                   sourceRange,
                   std::move(operand)),
        op(op) {}

    /// The operator of this expression.
    UnaryPrefixOperator operation() const { return op; }

    /// The operand of this expression.
    Expression* operand() { return child<Expression>(0); }

    /// \overload
    Expression const* operand() const { return child<Expression>(0); }

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
        Expression(NodeType::BinaryExpression,
                   sourceRange,
                   std::move(lhs),
                   std::move(rhs)),
        op(op) {}

    /// The operator of this expression.
    BinaryOperator operation() const { return op; }

    /// Change the operator of this expression.
    void setOperation(BinaryOperator newOp) { op = newOp; }

    /// The LHS operand of this expression.
    Expression* lhs() { return child<Expression>(0); }

    /// \overload
    Expression const* lhs() const { return child<Expression>(0); }

    /// The RHS operand of this expression.
    Expression* rhs() { return child<Expression>(1); }

    /// \overload
    Expression const* rhs() const { return child<Expression>(1); }

private:
    BinaryOperator op;
};

/// Concrete node representing a member access expression.
class SCATHA_API MemberAccess: public Expression {
public:
    explicit MemberAccess(UniquePtr<Expression> object,
                          UniquePtr<Identifier> member,
                          SourceRange sourceRange):
        Expression(NodeType::MemberAccess,
                   sourceRange,
                   std::move(object),
                   std::move(member)) {}

    /// The object of this expression.
    Expression* object() { return child<Expression>(0); }

    /// \overload
    Expression const* object() const { return child<Expression>(0); }

    /// The identifier to access the object.
    Identifier* member() { return child<Identifier>(1); }

    /// \overload
    Identifier const* member() const { return child<Identifier>(1); }
};

/// Concrete node representing a reference expression.
class SCATHA_API ReferenceExpression: public Expression {
public:
    explicit ReferenceExpression(UniquePtr<Expression> referred,
                                 SourceRange sourceRange):
        Expression(NodeType::ReferenceExpression,
                   sourceRange,
                   std::move(referred)) {}

    /// The object being referred to.
    Expression* referred() { return child<Expression>(0); }

    /// \overload
    Expression const* referred() const { return child<Expression>(0); }
};

/// Concrete node representing a `unique` expression.
class SCATHA_API UniqueExpression: public Expression {
public:
    explicit UniqueExpression(UniquePtr<Expression> initExpr,
                              SourceRange sourceRange):
        Expression(NodeType::UniqueExpression,
                   sourceRange,
                   std::move(initExpr)) {}

    /// The initializing expression
    Expression* initExpr() { return child<Expression>(0); }

    /// \overload
    Expression const* initExpr() const { return child<Expression>(0); }

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
        Expression(NodeType::Conditional,
                   sourceRange,
                   std::move(condition),
                   std::move(ifExpr),
                   std::move(elseExpr)) {}

    /// The condition to branch on.
    Expression* condition() { return child<Expression>(0); }

    /// \overload
    Expression const* condition() const { return child<Expression>(0); }

    /// Expression to evaluate if condition is true.
    Expression* thenExpr() { return child<Expression>(1); }

    /// \overload
    Expression const* thenExpr() const { return child<Expression>(1); }

    /// Expression to evaluate if condition is false.
    Expression* elseExpr() { return child<Expression>(2); }

    /// \overload
    Expression const* elseExpr() const { return child<Expression>(2); }
};

/// MARK: More Complex Expressions

/// Concrete node representing a function call expression.
class SCATHA_API CallLike: public Expression {
public:
    /// The object (function or rather overload set) being called.
    Expression* object() { return child<Expression>(0); }

    /// \overload
    Expression const* object() const { return child<Expression>(0); }

    /// List of arguments.
    auto arguments() { return dropChildren<Expression>(1); }

    /// \overload
    auto arguments() const { return dropChildren<Expression const>(1); }

protected:
    explicit CallLike(NodeType nodeType,
                      UniquePtr<Expression> object,
                      utl::small_vector<UniquePtr<Expression>> arguments,
                      SourceRange sourceRange):
        Expression(nodeType,
                   sourceRange,
                   std::move(object),
                   std::move(arguments)) {}
};

/// Concrete node representing a function call expression.
class SCATHA_API FunctionCall: public CallLike {
public:
    explicit FunctionCall(UniquePtr<Expression> object,
                          utl::small_vector<UniquePtr<Expression>> arguments,
                          SourceRange sourceRange):
        CallLike(NodeType::FunctionCall,
                 std::move(object),
                 std::move(arguments),
                 sourceRange) {}

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

    bool isMemberCall = false;
};

/// Concrete node representing a subscript expression.
class SCATHA_API Subscript: public CallLike {
public:
    explicit Subscript(UniquePtr<Expression> object,
                       utl::small_vector<UniquePtr<Expression>> arguments,
                       SourceRange sourceRange):
        CallLike(NodeType::Subscript,
                 std::move(object),
                 std::move(arguments),
                 sourceRange) {}
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
        Expression(NodeType::ListExpression, sourceRange, std::move(elems)) {}

    auto elements() { return children<Expression>(); }

    auto elements() const { return children<Expression>(); }
};

/// Abstract node representing a statement.
class SCATHA_API ImplicitConversion: public Expression {
public:
    explicit ImplicitConversion(UniquePtr<Expression> expr,
                                SourceRange sourceRange):
        Expression(NodeType::ImplicitConversion, sourceRange, std::move(expr)) {
    }

    /// The expression being converted
    Expression* expr() { return child<Expression>(0); }

    /// \overload
    Expression const* expr() const { return child<Expression>(0); }
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
        return nameIdentifier() ? nameIdentifier()->value() :
                                  std::string_view{};
    }

    /// Identifier expression representing the name of this declaration.
    Identifier* nameIdentifier() { return child<Identifier>(0); }

    /// \overload
    Identifier const* nameIdentifier() const { return child<Identifier>(0); }

    /// Access specifier. `None` if none was specified.
    AccessSpec accessSpec() const { return _accessSpec; }

    void setAccessSpec(AccessSpec spec) { _accessSpec = spec; }

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
    using Statement::Statement;

private:
    AccessSpec _accessSpec = AccessSpec::None;
    sema::Entity* _entity  = nullptr;
};

/// Concrete node representing a translation unit.
class SCATHA_API TranslationUnit: public AbstractSyntaxTree {
public:
    TranslationUnit(utl::small_vector<UniquePtr<Declaration>> declarations):
        AbstractSyntaxTree(NodeType::TranslationUnit,
                           SourceRange{},
                           std::move(declarations)) {}

    /// List of declarations in the translation unit.
    auto declarations() { return children<Declaration>(); }

    /// \overload
    auto declarations() const { return children<Declaration>(); }

    template <typename D = Declaration>
    D* declaration(size_t index) {
        return child<D>(index);
    }

    template <typename D = Declaration>
    D const* declaration(size_t index) const {
        return child<D>(index);
    }
};

/// Concrete node representing a variable declaration.
class SCATHA_API VariableDeclaration: public Declaration {
public:
    explicit VariableDeclaration(SourceRange sourceRange,
                                 UniquePtr<Identifier> name,
                                 UniquePtr<Expression> typeExpr,
                                 UniquePtr<Expression> initExpr):
        Declaration(NodeType::VariableDeclaration,
                    sourceRange,
                    std::move(name),
                    std::move(typeExpr),
                    std::move(initExpr)) {}

    /// Typename declared in the source code. Null if no typename was declared.
    Expression* typeExpr() { return child<Expression>(1); }

    /// \overload
    Expression const* typeExpr() const { return child<Expression>(1); }

    /// Expression to initialize this variable. May be null.
    Expression* initExpression() { return child<Expression>(2); }

    /// \overload
    Expression const* initExpression() const { return child<Expression>(2); }

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
    Expression* typeExpr() { return child<Expression>(1); }

    /// \overload
    Expression const* typeExpr() const { return child<Expression>(1); }

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
        Declaration(nodeType,
                    sourceRange,
                    std::move(name),
                    std::move(typeExpr)) {
        if (nameIdentifier()) {
            setSourceRange(nameIdentifier()->sourceRange());
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
        Statement(NodeType::CompoundStatement,
                  sourceRange,
                  std::move(statements)) {}

    /// List of statements in the compound statement.
    auto statements() { return children<Statement>(); }

    /// \overload
    auto statements() const { return children<Statement>(); }

    template <typename S = Statement>
    S* statement(size_t index) {
        return child<S>(index);
    }

    template <typename S = Statement>
    S const* statement(size_t index) const {
        return child<S>(index);
    }

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
    explicit FunctionDefinition(
        SourceRange sourceRange,
        UniquePtr<Identifier> name,
        utl::small_vector<UniquePtr<ParameterDeclaration>> parameters,
        UniquePtr<Expression> returnTypeExpr,
        UniquePtr<CompoundStatement> body):
        Declaration(NodeType::FunctionDefinition,
                    sourceRange,
                    std::move(name),
                    std::move(returnTypeExpr),
                    std::move(body),
                    std::move(parameters)) {}

    /// Typename of the return type as declared in the source code.
    /// Will be `nullptr` if no return type was declared.
    Expression* returnTypeExpr() { return child<Expression>(1); }

    /// \overload
    Expression const* returnTypeExpr() const { return child<Expression>(1); }

    /// Body of the function.
    CompoundStatement* body() { return child<CompoundStatement>(2); }

    /// \overload
    CompoundStatement const* body() const {
        return child<CompoundStatement const>(2);
    }

    /// List of parameter declarations.
    auto parameters() { return dropChildren<ParameterDeclaration>(3); }

    /// \overload
    auto parameters() const {
        return dropChildren<ParameterDeclaration const>(3);
    }

    template <typename P = ParameterDeclaration>
    P* parameter(size_t index) {
        return child<P>(index + 3);
    }

    template <typename P = ParameterDeclaration>
    P const* parameter(size_t index) const {
        return child<P>(index + 3);
    }

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
        Declaration(NodeType::StructDefinition,
                    sourceRange,
                    std::move(name),
                    std::move(body)) {}

    /// Body of the struct.
    CompoundStatement* body() { return child<CompoundStatement>(1); }

    /// \overload
    CompoundStatement const* body() const {
        return child<CompoundStatement>(1);
    }

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
                  expression ? expression->sourceRange() : SourceRange{},
                  std::move(expression)) {}

    /// The expression
    Expression* expression() { return child<Expression>(0); }

    /// \overload
    Expression const* expression() const { return child<Expression>(0); }
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
        ControlFlowStatement(NodeType::ReturnStatement,
                             sourceRange,
                             std::move(expression)) {}

    /// The returned expression. May be null in case of a void function.
    Expression* expression() { return child<Expression>(0); }

    /// \overload
    Expression const* expression() const { return child<Expression>(0); }
};

/// Concrete node representing an if/else statement.
class SCATHA_API IfStatement: public ControlFlowStatement {
public:
    explicit IfStatement(SourceRange sourceRange,
                         UniquePtr<Expression> condition,
                         UniquePtr<Statement> ifBlock,
                         UniquePtr<Statement> elseBlock):
        ControlFlowStatement(NodeType::IfStatement,
                             sourceRange,
                             std::move(condition),
                             std::move(ifBlock),
                             std::move(elseBlock)) {}

    /// Condition to branch on.
    /// Must not be null after parsing and must be of type bool (or maybe later
    /// convertible to bool).
    Expression* condition() { return child<Expression>(0); }

    /// \overload
    Expression const* condition() const { return child<Expression>(0); }

    /// Statement to execute if condition is true.
    Statement* thenBlock() { return child<Statement>(1); }

    /// \overload
    Statement const* thenBlock() const { return child<Statement>(1); }

    /// Statement to execute if condition is true.
    Statement* elseBlock() { return child<Statement>(2); }

    /// \overload
    Statement const* elseBlock() const { return child<Statement>(2); }
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
        ControlFlowStatement(NodeType::LoopStatement,
                             sourceRange,
                             std::move(varDecl),
                             std::move(condition),
                             std::move(increment),
                             std::move(block)),
        _kind(kind) {}

    /// Loop variable declared in this statement.
    /// Only non-null if `kind() == For`
    VariableDeclaration* varDecl() { return child<VariableDeclaration>(0); }

    /// \overload
    VariableDeclaration const* varDecl() const {
        return child<VariableDeclaration>(0);
    }

    /// Condition to loop on.
    Expression* condition() { return child<Expression>(1); }

    /// \overload
    Expression const* condition() const { return child<Expression>(1); }

    /// Increment expression
    /// Only non-null if `kind() == For`
    Expression* increment() { return child<Expression>(2); }

    /// \overload
    Expression const* increment() const { return child<Expression>(2); }

    /// Statement to execute repeatedly.
    CompoundStatement* block() { return child<CompoundStatement>(3); }

    /// \overload
    CompoundStatement const* block() const {
        return child<CompoundStatement>(3);
    }

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
