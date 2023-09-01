#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <range/v3/view.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/SourceLocation.h"
#include "Common/UniquePtr.h"
#include "Sema/DTorStack.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

// ASTNode
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
//    ├─ UnaryExpression
//    ├─ BinaryExpression
//    ├─ MemberAccess
//    ├─ Conditional
//    ├─ FunctionCall
//    ├─ Conversion
//    └─ Subscript

#define AST_DERIVED_COMMON(Type)                                               \
    UniquePtr<Type> extractFromParent() {                                      \
        auto* node = this->ASTNode::extractFromParent().release();             \
        return UniquePtr<Type>(cast<Type*>(node));                             \
    }

/// Defines a property such as `lhs()` on a `BinaryExpression`. These are very
/// verbose and repetative so we use a macro here
///
/// We define accessor templates also as non-templates for the debugger to
/// access them
#define AST_PROPERTY(Index, Type, Name, CapName)                               \
    Type* Name() { return this->Name<Type>(); }                                \
    template <std::derived_from<Type> TYPE>                                    \
    TYPE* Name() {                                                             \
        return this->ASTNode::child<TYPE>(Index);                              \
    }                                                                          \
                                                                               \
    Type const* Name() const { return this->Name<Type>(); }                    \
    template <std::derived_from<Type> TYPE>                                    \
    TYPE const* Name() const {                                                 \
        return this->ASTNode::child<TYPE>(Index);                              \
    }                                                                          \
                                                                               \
    template <std::derived_from<Type> TYPE = Type>                             \
    UniquePtr<TYPE> extract##CapName() {                                       \
        return this->ASTNode::extractChild<TYPE>(Index);                       \
    }                                                                          \
                                                                               \
    void set##CapName(UniquePtr<Type> node) {                                  \
        return this->ASTNode::setChild(Index, std::move(node));                \
    }

#define AST_RANGE_PROPERTY(BeginIndex, Type, Name, CapName)                    \
    template <std::derived_from<Type> TYPE = Type>                             \
    auto Name##s() {                                                           \
        return this->ASTNode::dropChildren<TYPE>(BeginIndex);                  \
    }                                                                          \
                                                                               \
    template <std::derived_from<Type> TYPE = Type>                             \
    auto Name##s() const {                                                     \
        return this->ASTNode::dropChildren<TYPE const>(BeginIndex);            \
    }                                                                          \
                                                                               \
    Type* Name(size_t index) { return this->Name<Type>(index); }               \
    template <std::derived_from<Type> TYPE>                                    \
    TYPE* Name(size_t index) {                                                 \
        return this->ASTNode::child<TYPE>(BeginIndex + index);                 \
    }                                                                          \
                                                                               \
    Type const* Name(size_t index) const { return this->Name<Type>(index); }   \
    template <std::derived_from<Type> TYPE = Type>                             \
    TYPE const* Name(size_t index) const {                                     \
        return this->ASTNode::child<TYPE>(BeginIndex + index);                 \
    }                                                                          \
                                                                               \
    template <std::derived_from<Type> TYPE = Type>                             \
    UniquePtr<TYPE> extract##CapName(size_t index) {                           \
        return this->ASTNode::extractChild<TYPE>(BeginIndex + index);          \
    }                                                                          \
                                                                               \
    void set##CapName(size_t index, UniquePtr<Type> node) {                    \
        return this->ASTNode::setChild(BeginIndex + index, std::move(node));   \
    }

namespace scatha::ast {

namespace internal {

class Decoratable {
public:
    bool isDecorated() const { return decorated; }

protected:
    void expectDecorated() const {
        SC_ASSERT(isDecorated(), "Requested decoration on undecorated node.");
    }
    void markDecorated() { decorated = true; }

private:
    bool decorated = false;
};

} // namespace internal

/// ## Base class for all nodes in the AST
/// Every derived class must specify its runtime type in the constructor via the
/// `NodeType` enum.
class SCATHA_API ASTNode: public internal::Decoratable {
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
    ASTNode() = default;

    /// AST nodes are not value types
    ASTNode(ASTNode const&) = delete;

    /// Runtime type of this node
    NodeType nodeType() const { return _type; }

    /// Source range object associated with this node.
    SourceRange sourceRange() const { return _sourceRange; }

    /// Extended source range of this node.
    /// This is the union of the source range of this node and all child nodes
    SourceRange extSourceRange() const;

    /// Source location object associated with this node.
    SourceLocation sourceLocation() const { return _sourceRange.begin(); }

    /// The parent of this node
    ASTNode* parent() { return _parent; }

    /// \overload
    ASTNode const* parent() const { return _parent; }

    /// The children of this node
    template <typename AST = ASTNode>
    auto children() {
        return getChildren<AST>();
    }

    /// \overload
    template <typename AST = ASTNode>
    auto children() const {
        return getChildren<AST const>();
    }

    /// The child at index \p index
    template <typename AST = ASTNode>
    AST* child(size_t index) {
        return cast_or_null<AST*>(children()[utl::narrow_cast<ssize_t>(index)]);
    }

    /// \overload
    template <typename AST = ASTNode>
    AST const* child(size_t index) const {
        return cast_or_null<AST const*>(
            children()[utl::narrow_cast<ssize_t>(index)]);
    }

    /// Extract the the child at index \p index
    template <typename AST = ASTNode>
    UniquePtr<AST> extractChild(size_t index) {
        auto* child = _children[index].release();
        child->_parent = nullptr;
        return UniquePtr<AST>(cast_or_null<AST*>(child));
    }

    /// Insert node \p child at position \p index
    /// Children at indices equal to and above \p index will be moved up
    void insertChild(size_t index, UniquePtr<ASTNode> child) {
        child->_parent = this;
        _children.insert(_children.begin() + index, std::move(child));
    }

    /// Set the the child at index \p index to \p child
    void setChild(size_t index, UniquePtr<ASTNode> child) {
        child->_parent = this;
        _children[index] = std::move(child);
    }

    /// Extract this node from its parent
    UniquePtr<ASTNode> extractFromParent();

    /// Replace the child \p old with new node \p repl
    /// The old child is destroyed
    /// \pre \p old must be a child of this node
    void replaceChild(ASTNode const* old, UniquePtr<ASTNode> repl);

    /// Get the index of child \p child
    /// \pre \p child must be a child of this node
    size_t indexOf(ASTNode const* child) const;

    /// Get the index of this node in its parent
    size_t indexInParent() const { return parent()->indexOf(this); }

protected:
    explicit ASTNode(NodeType type,
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

    friend void ast::privateDelete(ast::ASTNode*);

    NodeType _type;
    SourceRange _sourceRange;
    ASTNode* _parent = nullptr;
    utl::small_vector<UniquePtr<ASTNode>> _children;
};

// For `dyncast` compatibilty
NodeType dyncast_get_type(std::derived_from<ASTNode> auto const& node) {
    return node.nodeType();
}

/// MARK: Expressions

/// Abstract node representing any expression.
class SCATHA_API Expression: public ASTNode {
public:
    using ASTNode::ASTNode;

    AST_DERIVED_COMMON(Expression)

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

    /// Declared object
    /// This is equivalent to `cast<ObjType*>(entity())`
    template <typename ObjType = sema::Object>
    ObjType* object() {
        return const_cast<ObjType*>(std::as_const(*this).object<ObjType>());
    }

    /// \overload
    template <typename ObjType = sema::Object>
    ObjType const* object() const {
        return cast<ObjType const*>(entity());
    }

    /// The type of the expression. Only valid if: `kind == ::Value`
    sema::QualType type() const {
        expectDecorated();
        return _type;
    }

    /// The type of the expression, if this is a value. If this is a
    /// type, returns that type
    sema::QualType typeOrTypeEntity() const;

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
                  sema::QualType type = nullptr,
                  std::optional<sema::ValueCategory> valueCat = std::nullopt,
                  std::optional<sema::EntityCategory> entityCat = std::nullopt);

    /// \Returns Constant value if this expression is constant evaluable
    /// `nullptr` otherwise
    sema::Value const* constantValue() const { return constVal.get(); }

    /// Set the constant value of this expression
    void setConstantValue(UniquePtr<sema::Value> value) {
        constVal = std::move(value);
    }

private:
    sema::Entity* _entity = nullptr;
    sema::QualType _type = nullptr;
    sema::ValueCategory _valueCat = sema::ValueCategory::None;
    sema::EntityCategory _entityCat = sema::EntityCategory::Indeterminate;
    UniquePtr<sema::Value> constVal;
};

/// Concrete node representing an identifier.
class SCATHA_API Identifier: public Expression {
public:
    explicit Identifier(SourceRange sourceRange, std::string id):
        Expression(NodeType::Identifier, sourceRange), _value(id) {}

    AST_DERIVED_COMMON(Identifier)

    /// Literal string value as declared in the source.
    std::string const& value() const { return _value; }

private:
    std::string _value;
};

/// Concrete node representing a literal.
class SCATHA_API Literal: public Expression {
public:
    using ValueType = std::variant<APInt, APFloat, std::string>;

    explicit Literal(SourceRange sourceRange,
                     LiteralKind kind,
                     ValueType value):
        Expression(NodeType::Literal, sourceRange),
        _kind(kind),
        _value(std::move(value)) {}

    AST_DERIVED_COMMON(Literal)

    LiteralKind kind() const { return _kind; }

    ValueType const& value() const { return _value; };

    template <typename T>
    auto value() const {
        return std::get<T>(_value);
    };

private:
    LiteralKind _kind;
    ValueType _value;
};

/// MARK: Unary Expressions

/// Concrete node representing a unary prefix expression.
class SCATHA_API UnaryExpression: public Expression {
public:
    explicit UnaryExpression(UnaryOperator op,
                             UnaryOperatorNotation notation,
                             UniquePtr<Expression> operand,
                             SourceRange sourceRange):
        Expression(NodeType::UnaryExpression, sourceRange, std::move(operand)),
        op(op),
        no(notation) {}

    AST_DERIVED_COMMON(UnaryExpression)

    /// The operator of this expression.
    UnaryOperator operation() const { return op; }

    /// The notation of this expression.
    UnaryOperatorNotation notation() const { return no; }

    /// The operand of this expression.
    AST_PROPERTY(0, Expression, operand, Operand)

private:
    UnaryOperator op;
    UnaryOperatorNotation no;
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

    AST_DERIVED_COMMON(BinaryExpression)

    /// The operator of this expression.
    BinaryOperator operation() const { return op; }

    /// Change the operator of this expression.
    void setOperation(BinaryOperator newOp) { op = newOp; }

    /// The LHS operand of this expression.
    AST_PROPERTY(0, Expression, lhs, LHS)

    /// The RHS operand of this expression.
    AST_PROPERTY(1, Expression, rhs, RHS)

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

    AST_DERIVED_COMMON(MemberAccess)

    /// The object of this expression.
    AST_PROPERTY(0, Expression, object, Object)

    /// The identifier to access the object.
    AST_PROPERTY(1, Identifier, member, Member)
};

/// Concrete node representing a reference expression.
class SCATHA_API ReferenceExpression: public Expression {
public:
    explicit ReferenceExpression(UniquePtr<Expression> referred,
                                 bool isMut,
                                 SourceRange sourceRange):
        Expression(NodeType::ReferenceExpression,
                   sourceRange,
                   std::move(referred)),
        isMut(isMut) {}

    AST_PROPERTY(0, Expression, referred, Referred)

    /// `true` if reference to `mut`
    bool isMutable() const { return isMut; }

private:
    bool isMut = true;
};

/// Concrete node representing a `unique` expression.
class SCATHA_API UniqueExpression: public Expression {
public:
    explicit UniqueExpression(UniquePtr<Expression> initExpr,
                              SourceRange sourceRange):
        Expression(NodeType::UniqueExpression,
                   sourceRange,
                   std::move(initExpr)) {}

    AST_DERIVED_COMMON(UniqueExpression)

    /// The initializing expression
    AST_PROPERTY(0, Expression, initExpr, InitExpr)

    /// Decorate this node.
    void decorate(sema::QualType type) { Expression::decorate(nullptr, type); }
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

    AST_DERIVED_COMMON(Conditional)

    /// The condition to branch on
    AST_PROPERTY(0, Expression, condition, Condition)

    /// Expression to evaluate if condition is true
    AST_PROPERTY(1, Expression, thenExpr, ThenExpr)

    /// Expression to evaluate if condition is false
    AST_PROPERTY(2, Expression, elseExpr, ElseExpr)
};

/// MARK: More Complex Expressions

/// Concrete node representing a function call expression.
class SCATHA_API CallLike: public Expression {
public:
    /// The object (function or rather overload set) being called.
    AST_PROPERTY(0, Expression, object, Object)

    /// List of arguments.
    AST_RANGE_PROPERTY(1, Expression, argument, Argument);

    /// Insert node \p arg as an argument of this call
    void insertArgument(size_t index, UniquePtr<ASTNode> child) {
        insertChild(index + 1, std::move(child));
    }

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

    AST_DERIVED_COMMON(FunctionCall)

    /// **Decoration provided by semantic analysis**

    /// The resolved function.
    /// Differs from object because the object refers to the overload
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
class SCATHA_API Subscript: public CallLike {
public:
    explicit Subscript(UniquePtr<Expression> object,
                       utl::small_vector<UniquePtr<Expression>> arguments,
                       SourceRange sourceRange):
        CallLike(NodeType::Subscript,
                 std::move(object),
                 std::move(arguments),
                 sourceRange) {}

    AST_DERIVED_COMMON(Subscript)
};

/// Concrete node representing a generic expression.
class SCATHA_API GenericExpression: public CallLike {
public:
    explicit GenericExpression(
        UniquePtr<Expression> object,
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange):
        CallLike(NodeType::GenericExpression,
                 std::move(object),
                 std::move(arguments),
                 sourceRange) {}

    AST_DERIVED_COMMON(GenericExpression)
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

    /// The elements in this list
    AST_RANGE_PROPERTY(0, Expression, element, Element);
};

/// Abstract node representing a statement.
class SCATHA_API Statement: public ASTNode {
public:
    using ASTNode::ASTNode;

    AST_DERIVED_COMMON(Statement)

    /// \Returns the destructor stack associated with this statement
    sema::DTorStack& dtorStack() { return _dtorStack; }

    /// \overload
    sema::DTorStack const& dtorStack() const { return _dtorStack; }

private:
    sema::DTorStack _dtorStack;
};

/// Abstract node representing a declaration.
class SCATHA_API Declaration: public Statement {
public:
    /// Name of the declared symbol as written in the source code.
    std::string_view name() const {
        return nameIdentifier() ? nameIdentifier()->value() :
                                  std::string_view{};
    }

    AST_DERIVED_COMMON(Declaration)

    /// Identifier expression representing the name of this declaration.
    AST_PROPERTY(0, Identifier, nameIdentifier, NameIdentifier)

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
    sema::Entity* _entity = nullptr;
};

/// Concrete node representing a translation unit.
class SCATHA_API TranslationUnit: public ASTNode {
public:
    TranslationUnit(utl::small_vector<UniquePtr<Declaration>> declarations):
        ASTNode(NodeType::TranslationUnit,
                SourceRange{},
                std::move(declarations)) {}

    AST_DERIVED_COMMON(TranslationUnit)

    /// List of declarations in the translation unit.
    AST_RANGE_PROPERTY(0, Declaration, declaration, Declaration)
};

/// Concrete node representing a variable declaration.
class SCATHA_API VariableDeclaration: public Declaration {
public:
    explicit VariableDeclaration(SourceRange sourceRange,
                                 UniquePtr<Identifier> name,
                                 UniquePtr<Expression> typeExpr,
                                 UniquePtr<Expression> initExpr,
                                 bool isMut):
        Declaration(NodeType::VariableDeclaration,
                    sourceRange,
                    std::move(name),
                    std::move(typeExpr),
                    std::move(initExpr)),
        isMut(isMut) {}

    AST_DERIVED_COMMON(VariableDeclaration)

    /// Typename declared in the source code. Null if no typename was declared.
    AST_PROPERTY(1, Expression, typeExpr, TypeExpr)

    /// Expression to initialize this variable. May be null.
    AST_PROPERTY(2, Expression, initExpression, InitExpression)

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
    sema::QualType type() const {
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

    /// `true` if this variable was declared with `let`, `false` if declared
    /// with `var`
    bool isMutable() const { return isMut; }

    /// Decorate this node.
    void decorate(sema::Entity* entity, sema::QualType type) {
        _type = type;
        Declaration::decorate(entity);
    }

    void setOffset(size_t offset) {
        _offset = utl::narrow_cast<uint32_t>(offset);
    }

    void setIndex(size_t index) { _index = utl::narrow_cast<uint32_t>(index); }

private:
    sema::QualType _type = nullptr;
    uint32_t _offset = 0;
    uint32_t _index : 31 = 0;
    bool isMut      : 1 = true;
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

    AST_DERIVED_COMMON(ParameterDeclaration)

    AST_PROPERTY(1, Expression, typeExpr, TypeExpr)

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

    /// Type of the parameter.
    sema::QualType type() const {
        expectDecorated();
        return _type;
    }

    /// Decorate this node.
    void decorate(sema::Entity* entity, sema::QualType type) {
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
    sema::QualType _type = nullptr;
};

/// Represents the explicit `this` parameter
class ThisParameter: public ParameterDeclaration {
public:
    explicit ThisParameter(SourceRange sourceRange,
                           std::optional<sema::Reference> ref,
                           sema::Mutability mut):
        ParameterDeclaration(NodeType::ThisParameter,
                             sourceRange,
                             nullptr,
                             nullptr),
        ref(ref),
        mut(mut) {}

    AST_DERIVED_COMMON(ThisParameter)

    /// The optional reference qualifier attached to the `this` parameter
    std::optional<sema::Reference> reference() const { return ref; }

    /// The mutability qualifier attached to the `this` parameter
    sema::Mutability mutability() const { return mut; }

private:
    std::optional<sema::Reference> ref;
    sema::Mutability mut;
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

    AST_DERIVED_COMMON(CompoundStatement)

    /// List of statements in the compound statement.
    AST_RANGE_PROPERTY(0, Statement, statement, Statement)

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

    AST_DERIVED_COMMON(EmptyStatement)
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

    AST_DERIVED_COMMON(FunctionDefinition)

    /// Typename of the return type as declared in the source code.
    /// Will be `nullptr` if no return type was declared.
    AST_PROPERTY(1, Expression, returnTypeExpr, ReturnTypeExpr)

    /// Body of the function.
    AST_PROPERTY(2, CompoundStatement, body, Body)

    /// List of parameter declarations.
    AST_RANGE_PROPERTY(3, ParameterDeclaration, parameter, Parameter)

    /// The `this` parameter of this function, or `nullptr` if nonesuch exists
    ThisParameter* thisParameter() {
        return const_cast<ThisParameter*>(std::as_const(*this).thisParameter());
    }

    /// \overload
    ThisParameter const* thisParameter() const {
        if (parameters().empty()) {
            return nullptr;
        }
        return dyncast<ThisParameter const*>(parameter(0));
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
    sema::QualType returnType() const {
        expectDecorated();
        return _returnType;
    }

    /// Decorate this node.
    void decorate(sema::Entity* entity, sema::QualType returnType) {
        _returnType = returnType;
        Declaration::decorate(entity);
    }

private:
    sema::QualType _returnType = nullptr;
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

    AST_DERIVED_COMMON(StructDefinition)

    /// Body of the struct.
    AST_PROPERTY(1, CompoundStatement, body, Body)

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

/// Abstract node representing any control flow statement like `if`, `while`,
/// `for`, `return` etc. May only appear at function scope.
class SCATHA_API ControlFlowStatement: public Statement {
protected:
    using Statement::Statement;

    AST_DERIVED_COMMON(ControlFlowStatement)
};

/// Concrete node representing a return statement.
class SCATHA_API ReturnStatement: public ControlFlowStatement {
public:
    explicit ReturnStatement(SourceRange sourceRange,
                             UniquePtr<Expression> expression):
        ControlFlowStatement(NodeType::ReturnStatement,
                             sourceRange,
                             std::move(expression)) {}

    AST_DERIVED_COMMON(ReturnStatement)

    /// The returned expression. May be null in case of a void function.
    AST_PROPERTY(0, Expression, expression, Expression)
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

    AST_DERIVED_COMMON(IfStatement)

    /// Condition to branch on
    AST_PROPERTY(0, Expression, condition, Condition)

    /// Statement to execute if condition is satisfied
    AST_PROPERTY(1, Statement, thenBlock, ThenBlock)

    /// Statement to execute if condition is not satisfied. May be null
    AST_PROPERTY(2, Statement, elseBlock, ElseBlock)
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

    AST_DERIVED_COMMON(LoopStatement)

    /// Loop variable declared in this statement.
    /// Only non-null if `kind() == For`
    AST_PROPERTY(0, VariableDeclaration, varDecl, VarDecl)

    /// Condition to loop on.
    AST_PROPERTY(1, Expression, condition, Condition)

    /// Increment expression
    /// Only non-null if `kind() == For`
    AST_PROPERTY(2, Expression, increment, Increment)

    /// Statement to execute repeatedly.
    AST_PROPERTY(3, CompoundStatement, block, Block)

    /// Either `while` or `do`/`while`
    LoopKind kind() const { return _kind; }

    /// The destructor stack of the loop condition
    sema::DTorStack& conditionDtorStack() { return _condDtors; }

    /// \overload
    sema::DTorStack const& conditionDtorStack() const { return _condDtors; }

    /// The destructor stack of the loop increment expression
    sema::DTorStack& incrementDtorStack() { return _incDtors; }

    /// \overload
    sema::DTorStack const& incrementDtorStack() const { return _incDtors; }

private:
    LoopKind _kind;
    sema::DTorStack _condDtors;
    sema::DTorStack _incDtors;
};

/// Represents a `break` or `continue` statement.
class SCATHA_API JumpStatement: public ControlFlowStatement {
public:
    enum Kind { Break, Continue };

    explicit JumpStatement(Kind kind, SourceRange sourceRange):
        ControlFlowStatement(NodeType::JumpStatement, sourceRange),
        _kind(kind) {}

    AST_DERIVED_COMMON(JumpStatement)

    Kind kind() const { return _kind; }

private:
    Kind _kind;
};

/// # Synthetical nodes inserted by Sema

/// Concrete node representing a type conversion
class SCATHA_API Conversion: public Expression {
public:
    explicit Conversion(UniquePtr<Expression> expr,
                        std::unique_ptr<sema::Conversion> conv);

    ~Conversion();

    AST_DERIVED_COMMON(Conversion)

    /// The target type of the conversion
    sema::QualType targetType() const;

    /// The conversion
    sema::Conversion const* conversion() const { return _conv.get(); }

    /// The expression being converted
    AST_PROPERTY(0, Expression, expression, Expression)

private:
    std::unique_ptr<sema::Conversion> _conv;
};

/// Concrete node representing a lifetime function call
class SCATHA_API ConstructorCall: public Expression {
public:
    explicit ConstructorCall(std::span<UniquePtr<Expression>> arguments,
                             sema::Function* lifetimeFunction,
                             sema::SpecialMemberFunction kind):
        Expression(NodeType::ConstructorCall, SourceRange{}, arguments),
        _function(lifetimeFunction),
        _kind(kind) {}

    AST_DERIVED_COMMON(ConstructorCall)

    AST_RANGE_PROPERTY(0, Expression, argument, Argument)

    /// The lifetime function to call
    sema::Function* function() const { return _function; }

    /// The type being constructed.
    /// \Returns `cast<ObjectType const*>(function()->parent())`
    sema::ObjectType const* constructedType() const;

    /// The kind of lifetime function
    sema::SpecialMemberFunction kind() const { return _kind; }

private:
    sema::Function* _function;
    sema::SpecialMemberFunction _kind;
};

} // namespace scatha::ast

#undef AST_DERIVED_COMMON
#undef AST_PROPERTY
#undef AST_RANGE_PROPERTY

#endif // SCATHA_AST_AST_H_
