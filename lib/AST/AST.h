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
#include "AST/SpecifierList.h"
#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/SourceLocation.h"
#include "Common/UniquePtr.h"
#include "Sema/CleanupStack.h"
#include "Sema/Fwd.h"
#include "Sema/LifetimeOperation.h"
#include "Sema/QualType.h"

/// # AST class hierarchy
/// ```
/// ASTNode
/// ├─ TranslationUnit
/// ├─ Statement
/// │  ├─ Declaration
/// │  │  ├─ VariableDeclaration
/// │  │  ├─ ParameterDeclaration
/// │  │  │  └─ ThisParameter
/// │  │  ├─ ModuleDeclaration
/// │  │  ├─ FunctionDefinition
/// │  │  └─ StructDefinition
/// │  ├─ CompoundStatement
/// │  ├─ ExpressionStatement
/// │  └─ ControlFlowStatement
/// │     ├─ ReturnStatement
/// │     ├─ IfStatement
/// │     ├─ LoopStatement
/// │     └─ JumpStatement
/// └─ Expression
///    ├─ Identifier
///    ├─ Literal
///    ├─ UnaryExpression
///    ├─ BinaryExpression
///    ├─ MemberAccess
///    ├─ Conditional
///    ├─ MoveExpr
///    ├─ CallLike
///    │  ├─ FunctionCall
///    │  ├─ Subscript
///    │  └─ ConstructExpr
///    ├─ NontrivAssignExpr
///    ├─ AddressOfExpression
///    ├─ DereferenceExpression
///    ├─ Conversion
///    └─ ConstructBase
///       ├─ TrivDefConstructExpr
///       ├─ TrivCopyConstructExpr
///       ├─ TrivAggrConstructExpr
///       ├─ NontrivAggrConstructExpr
///       ├─ NontrivConstructExpr
///       ├─ NontrivInlineConstructExpr
///       └─ DynArrayConstructExpr
/// ```

#define AST_DERIVED_COMMON(Type)                                               \
    UniquePtr<Type> extractFromParent() {                                      \
        auto* node = this -> ASTNode::extractFromParent().release();           \
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
    Type* set##CapName(UniquePtr<Type> node) {                                 \
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
    Type* set##CapName(size_t index, UniquePtr<Type> node) {                   \
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
        [](auto& p) -> AST* { return cast<AST*>(p.get()); });

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
    SourceRange directSourceRange() const { return _sourceRange; }

    ///
    void setDirectSourceRange(SourceRange range) { _sourceRange = range; }

    /// Entire source range of this node.
    SourceRange sourceRange() const;

    /// Source location object associated with this node.
    SourceLocation sourceLocation() const { return _sourceRange.begin(); }

    /// The parent of this node
    ASTNode* parent() { return _parent; }

    /// \overload
    template <typename N>
    N* parent() {
        return cast<N*>(parent());
    }

    /// \overload
    ASTNode const* parent() const { return _parent; }

    /// \overload
    template <typename N>
    N const* parent() const {
        return cast<N const*>(parent());
    }

    /// Search the ancestors of this node for a node of type \p Node
    /// \Returns that node if found, otherwise returns `nullptr`
    template <typename Node>
    Node* findAncestor() {
        return const_cast<Node*>(std::as_const(*this).findAncestor<Node>());
    }

    /// \overload for const
    template <typename Node>
    Node const* findAncestor() const {
        ASTNode const* node = this;
        while (true) {
            node = node->parent();
            if (!node) {
                return nullptr;
            }
            if (auto* result = dyncast<Node const*>(node)) {
                return result;
            }
        }
    }

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
        return cast<AST*>(children()[utl::narrow_cast<ssize_t>(index)]);
    }

    /// \overload
    template <typename AST = ASTNode>
    AST const* child(size_t index) const {
        return cast<AST const*>(children()[utl::narrow_cast<ssize_t>(index)]);
    }

    /// Extract the the child at index \p index
    template <typename AST = ASTNode>
    UniquePtr<AST> extractChild(size_t index) {
        auto* child = _children[index].release();
        if (child) {
            child->_parent = nullptr;
        }
        return UniquePtr<AST>(cast<AST*>(child));
    }

    /// Insert node \p child at position \p index
    /// Children at indices equal to and above \p index will be moved up
    template <typename AST = ASTNode>
    AST* insertChild(size_t index, UniquePtr<AST> child) {
        if (child) {
            child->_parent = this;
        }
        auto* result = child.get();
        _children.insert(_children.begin() + index, std::move(child));
        return result;
    }

    /// Set the the child at index \p index to \p child
    template <typename AST = ASTNode>
    AST* setChild(size_t index, UniquePtr<AST> child) {
        if (child) {
            child->_parent = this;
        }
        auto* result = child.get();
        _children[index] = std::move(child);
        return result;
    }

    /// Extract this node from its parent
    UniquePtr<ASTNode> extractFromParent();

    /// Replace the child \p old with new node \p repl
    /// The old child is destroyed
    /// \pre \p old must be a child of this node
    /// \Returns \p repl as a raw pointer
    ASTNode* replaceChild(ASTNode const* old, UniquePtr<ASTNode> repl);

    /// \overload for generic type
    template <typename T>
    T* replaceChild(ASTNode const* old, UniquePtr<T> repl) {
        return cast<T*>(
            replaceChild(old, uniquePtrCast<ASTNode>(std::move(repl))));
    }

    /// Replace `this` node with new node \p repl
    /// `this` is destroyed after the call
    /// \Returns \p repl as a raw pointer
    ASTNode* replace(UniquePtr<ASTNode> repl);

    /// \overload for generic type
    template <typename T>
    T* replace(UniquePtr<T> repl) {
        return cast<T*>(replace(uniquePtrCast<ASTNode>(std::move(repl))));
    }

    /// Get the index of child \p child
    /// \pre \p child must be a child of this node
    size_t indexOf(ASTNode const* child) const;

    /// Get the index of this node in its parent
    size_t indexInParent() const { return parent()->indexOf(this); }

protected:
    template <typename... C>
    explicit ASTNode(NodeType type, SourceRange sourceRange, C&&... children):
        _type(type), _sourceRange(sourceRange) {
        (addChildren(std::forward<C>(children)), ...);
    }

    void setSourceRange(SourceRange sourceRange) { _sourceRange = sourceRange; }

private:
    template <typename C>
    void addChildren(C&& child) {
        if constexpr (ranges::range<C>) {
            for (auto&& c: child) {
                addChildren(std::move(c));
            }
        }
        else {
            if (child) {
                child->_parent = this;
            }
            _children.push_back(std::forward<C>(child));
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

/// \overload
template <typename Node, std::invocable<UniquePtr<Node>> F>
auto* insertNode(Node* node, F&& constructor) {
    SC_EXPECT(node->parent());
    auto* parent = node->parent();
    size_t index = node->indexInParent();
    return parent->setChild(index, constructor(node->extractFromParent()));
}

/// Allocates an AST node of type \p T and inserts it as the parent of \p node
/// \p node is extracted and used as the first argument to construct \p T
/// \p args are the remaining arguments to construction of \p T
/// \pre \p node must have a parent
/// \Returns the allocated node
template <std::derived_from<ast::ASTNode> T, typename Node, typename... Args>
    requires std::constructible_from<T, UniquePtr<Node>, Args...>
T* insertNode(Node* node, Args&&... args) {
    return insertNode(node, [&](UniquePtr<Node>&& node) {
        return allocate<T>(std::move(node), std::forward<Args>(args)...);
    });
}

/// MARK: Expressions

/// Abstract node representing any expression.
class SCATHA_API Expression: public ASTNode {
public:
    using ASTNode::ASTNode;

    AST_DERIVED_COMMON(Expression)

    /// **Decoration provided by semantic analysis**

    /// Whether the expression denotes a value or a type.
    sema::EntityCategory entityCategory() const;

    /// The entity in the symbol table that this expression refers to. This must
    /// not be null after successful semantic analysis.
    sema::Entity* entity() {
        return const_cast<sema::Entity*>(std::as_const(*this).entity());
    }

    /// \overload
    sema::Entity const* entity() const {
        expectDecorated();
        return _entity;
    }

    /// Declared object
    /// This is equivalent to `cast<Object*>(entity())`
    sema::Object* object() {
        return const_cast<sema::Object*>(std::as_const(*this).object());
    }

    /// \overload
    sema::Object const* object() const;

    /// The type of the expression. Only non-null if: `kind == ::Value`
    sema::QualType type() const {
        expectDecorated();
        return _type;
    }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Value`
    bool isValue() const {
        return entityCategory() == sema::EntityCategory::Value;
    }

    /// \Returns the value category of this expression
    /// \pre This expression must refer to a value
    sema::ValueCategory valueCategory() const { return _valueCat; }

    /// Convenience wrapper for `valueCategory() == LValue`
    bool isLValue() const {
        return valueCategory() == sema::ValueCategory::LValue;
    }

    /// Convenience wrapper for `valueCategory() == RValue`
    bool isRValue() const {
        return valueCategory() == sema::ValueCategory::RValue;
    }

    /// Convenience wrapper for: `entityCategory() == EntityCategory::Type`
    bool isType() const {
        return entityCategory() == sema::EntityCategory::Type;
    }

    /// TODO: Make decorateValue() and decorateType() protected
    /// Then every concrete AST node has to decide whether to expose these
    /// functions or to provide their own

    /// Decorate this node if this node refers to a value.
    /// \param obejct The object in the symbol table that this expression refers
    /// to. Must not be null. \param valueCategory The value category of this
    /// expression. \param type The type of this expression. Right now in some
    /// cases this differs from the type of the object, but we want to change
    /// that and then remove this parameter
    void decorateValue(sema::Entity* entity, sema::ValueCategory valueCategory,
                       sema::QualType type = nullptr);

    /// Decorate this node if this node refers to a type
    void decorateType(sema::Type* type);

    /// Decorate this node if this node refers to a namespace
    void decorateNamespace(sema::Entity* entity);

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
    sema::ValueCategory _valueCat{};
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
    using ValueType = std::variant<std::monostate, APInt, APFloat, std::string>;

    explicit Literal(SourceRange sourceRange, LiteralKind kind,
                     ValueType value = std::monostate{}):
        Expression(NodeType::Literal, sourceRange),
        _kind(kind),
        _value(std::move(value)) {}

    AST_DERIVED_COMMON(Literal)

    LiteralKind kind() const { return _kind; }

    ValueType const& value() const { return _value; };

    template <typename T>
    auto value() const {
        return std::get<T>(_value);
    }

private:
    LiteralKind _kind;
    ValueType _value;
};

/// MARK: Unary Expressions

/// Concrete node representing a unary prefix expression.
class SCATHA_API UnaryExpression: public Expression {
public:
    explicit UnaryExpression(UnaryOperator op, UnaryOperatorNotation notation,
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
    explicit BinaryExpression(BinaryOperator op, UniquePtr<Expression> lhs,
                              UniquePtr<Expression> rhs,
                              SourceRange sourceRange):
        Expression(NodeType::BinaryExpression, sourceRange, std::move(lhs),
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
        Expression(NodeType::MemberAccess, sourceRange, std::move(object),
                   std::move(member)) {}

    AST_DERIVED_COMMON(MemberAccess)

    /// The object of this expression.
    AST_PROPERTY(0, Expression, accessed, Accessed)

    /// The identifier to access the object.
    AST_PROPERTY(1, Identifier, member, Member)
};

template <NodeType Type>
class SCATHA_API RefExprBase: public Expression {
public:
    explicit RefExprBase(UniquePtr<Expression> referred,
                         sema::Mutability mutability, SourceRange sourceRange):
        Expression(Type, sourceRange, std::move(referred)), mut(mutability) {}

    AST_PROPERTY(0, Expression, referred, Referred)

    /// \Returns the mutability qualifier
    sema::Mutability mutability() const { return mut; }

    /// \Returns `true` if reference to `mut`
    bool isMut() const { return mut == sema::Mutability::Mutable; }

private:
    sema::Mutability mut;
};

/// Concrete node representing a reference expression.
class SCATHA_API AddressOfExpression:
    public RefExprBase<NodeType::AddressOfExpression> {
public:
    using RefExprBase::RefExprBase;
};

/// Concrete node representing a dereference expression.
class SCATHA_API DereferenceExpression:
    public RefExprBase<NodeType::DereferenceExpression> {
public:
    explicit DereferenceExpression(UniquePtr<Expression> referred,
                                   sema::Mutability mut, bool unique,
                                   SourceRange sourceRange):
        RefExprBase(std::move(referred), mut, sourceRange), _unique(unique) {}

    /// \Returns `true` if this dereference expression has a `unique` token
    /// attached to it
    bool unique() const { return _unique; }

private:
    bool _unique;
};

/// MARK: Ternary Expressions

/// Concrete node representing a conditional expression.
class SCATHA_API Conditional: public Expression {
public:
    explicit Conditional(UniquePtr<Expression> condition,
                         UniquePtr<Expression> ifExpr,
                         UniquePtr<Expression> elseExpr,
                         SourceRange sourceRange):
        Expression(NodeType::Conditional, sourceRange, std::move(condition),
                   std::move(ifExpr), std::move(elseExpr)) {}

    AST_DERIVED_COMMON(Conditional)

    /// The condition to branch on
    AST_PROPERTY(0, Expression, condition, Condition)

    /// Expression to evaluate if condition is true
    AST_PROPERTY(1, Expression, thenExpr, ThenExpr)

    /// Expression to evaluate if condition is false
    AST_PROPERTY(2, Expression, elseExpr, ElseExpr)

    /// \Returns the cleanup stack associated with the branch at index \p index
    /// \Pre \p index must be 0 or 1
    sema::CleanupStack& branchCleanupStack(size_t index) {
        SC_EXPECT(index < 2);
        return branchCleanups[index];
    }

    /// \overload
    sema::CleanupStack const& branchCleanupStack(size_t index) const {
        return branchCleanups[index];
    }

private:
    sema::CleanupStack branchCleanups[2];
};

/// Move expression
class SCATHA_API MoveExpr: public Expression {
public:
    explicit MoveExpr(UniquePtr<Expression> value, SourceRange sourceRange):
        Expression(NodeType::MoveExpr, sourceRange, std::move(value)) {}

    AST_DERIVED_COMMON(MoveExpr)

    /// The moved value
    AST_PROPERTY(0, Expression, value, Value)

    /// \Returns the lifetime operation invoked by this move expression
    /// If this returns `std::nullopt`, then the move expression has no effect
    /// (because we are moving an rvalue)
    std::optional<sema::LifetimeOperation> operation() const { return op; }

    /// Sets the generated construct operation and decorates this expression
    void decorateMove(sema::Entity* entity,
                      std::optional<sema::LifetimeOperation> op) {
        decorateValue(entity, sema::ValueCategory::RValue);
        this->op = op;
    }

private:
    std::optional<sema::LifetimeOperation> op;
};

/// Unique expression
class SCATHA_API UniqueExpr: public Expression {
public:
    explicit UniqueExpr(UniquePtr<Expression> value, SourceRange sourceRange):
        Expression(NodeType::UniqueExpr, sourceRange, std::move(value)) {}

    AST_DERIVED_COMMON(UniqueExpr)

    /// The moved value
    AST_PROPERTY(0, Expression, value, Value)
};

/// MARK: More Complex Expressions

/// Abstract node representing a function-call-like expression.
/// This is the base class of `FunctionCall`, `Subscript` and `ConstructorCall`
class SCATHA_API CallLike: public Expression {
public:
    /// The object (function or rather overload set) being called.
    AST_PROPERTY(0, Expression, callee, Callee)

    /// List of arguments.
    AST_RANGE_PROPERTY(1, Expression, argument, Argument);

    /// Insert node \p arg as an argument of this call
    void insertArgument(size_t index, UniquePtr<ASTNode> child) {
        insertChild(index + 1, std::move(child));
    }

protected:
    explicit CallLike(NodeType nodeType, UniquePtr<Expression> callee,
                      utl::small_vector<UniquePtr<Expression>> arguments,
                      SourceRange sourceRange):
        Expression(nodeType, sourceRange, std::move(callee),
                   std::move(arguments)) {}
};

/// Concrete node representing a function call expression.
class SCATHA_API FunctionCall: public CallLike {
public:
    explicit FunctionCall(UniquePtr<Expression> callee,
                          utl::small_vector<UniquePtr<Expression>> arguments,
                          SourceRange sourceRange):
        CallLike(NodeType::FunctionCall, std::move(callee),
                 std::move(arguments), sourceRange) {}

    AST_DERIVED_COMMON(FunctionCall)

    /// **Decoration provided by semantic analysis**

    /// The resolved function.
    /// Differs from `callee()` because callee refers to the overload
    /// set
    sema::Function* function() { return _function; }

    /// \overload
    sema::Function const* function() const { return _function; }

    /// Decorate this function call
    void decorateCall(sema::Object* object, sema::ValueCategory valueCategory,
                      sema::QualType type, sema::Function* calledFunction);

private:
    sema::Function* _function = nullptr;
};

/// Concrete node representing a subscript expression.
class SCATHA_API Subscript: public CallLike {
public:
    explicit Subscript(UniquePtr<Expression> object,
                       utl::small_vector<UniquePtr<Expression>> arguments,
                       SourceRange sourceRange):
        CallLike(NodeType::Subscript, std::move(object), std::move(arguments),
                 sourceRange) {}

    AST_DERIVED_COMMON(Subscript)
};

/// Concrete node representing a subscript slice expression.
class SCATHA_API SubscriptSlice: public CallLike {
public:
    explicit SubscriptSlice(UniquePtr<Expression> object,
                            UniquePtr<Expression> lower,
                            UniquePtr<Expression> upper,
                            SourceRange sourceRange):
        CallLike(NodeType::SubscriptSlice, std::move(object),
                 toSmallVector(std::move(lower), std::move(upper)),
                 sourceRange) {}

    AST_DERIVED_COMMON(SubscriptSlice)

    /// The left hand side of ":"
    AST_PROPERTY(1, Expression, lower, Lower)

    /// The right hand side of ":"
    AST_PROPERTY(2, Expression, upper, Upper)
};

/// Concrete node representing a generic expression.
class SCATHA_API GenericExpression: public CallLike {
public:
    explicit GenericExpression(
        UniquePtr<Expression> object,
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange):
        CallLike(NodeType::GenericExpression, std::move(object),
                 std::move(arguments), sourceRange) {}

    AST_DERIVED_COMMON(GenericExpression)
};

/// Represents a list expression, i.e. `[a, b, c]`
class SCATHA_API ListExpression: public Expression {
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

    /// \Returns the stack of cleanup operations associated with this statement
    /// Cleanup operations can be destructor calls or inline object destruction
    /// like destroying array elements in a loop or deallocating unique pointers
    sema::CleanupStack const& cleanupStack() const { return _cleanupStack; }

    /// \overload
    sema::CleanupStack& cleanupStack() { return _cleanupStack; }

    /// **Decoration provided by semantic analysis**

    /// \Returns the entity this statement corresponds to
    sema::Entity* entity() {
        return const_cast<sema::Entity*>(std::as_const(*this).entity());
    }

    /// \overload
    sema::Entity const* entity() const {
        expectDecorated();
        return _entity;
    }

    /// Decorate this node.
    void decorateStmt(sema::Entity* entity) {
        _entity = entity;
        markDecorated();
    }

    /// \Returns `true` if this statement is reachable
    bool reachable() const { return _reachable; }

    /// Marks this statement unreachable. Does not update child statements
    void markUnreachable() { _reachable = false; }

private:
    bool _reachable = true;
    sema::Entity* _entity = nullptr;
    sema::CleanupStack _cleanupStack;
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

    /// Sets all explicit specifiers. This functions should only be used by the
    /// parser
    void setSpecifiers(SpecifierList specs) { _specs = std::move(specs); }

    /// \Returns the specified access control of this declaration or
    /// `std::nullopt` if none was specified
    std::optional<sema::AccessControl> accessControl() const {
        return _specs.accessControl();
    }

    /// Source range of the access control specifier
    SourceRange accessControlSourceRange() const {
        return _specs.accessControlSourceRange();
    }

    /// \Returns the 'LINKAGE' string if this declaration was declared
    /// with`extern "LINKAGE"`
    std::optional<std::string> externalLinkage() const {
        return _specs.externalLinkage();
    }

    /// Source range of the external linkage specifier
    SourceRange externalLinkageSourceRange() const {
        return _specs.externalLinkageSourceRange();
    }

    /// **Decoration provided by semantic analysis**

    /// Decorate this node.
    void decorateDecl(sema::Entity* entity) { decorateStmt(entity); }

protected:
    template <typename... C>
    explicit Declaration(NodeType type, SpecifierList specList,
                         SourceRange sourceRange, C&&... children):
        Statement(type, sourceRange, std::forward<C>(children)...),
        _specs(std::move(specList)) {}

private:
    SpecifierList _specs;
};

/// Concrete node representing the root of every AST
class SCATHA_API TranslationUnit: public ASTNode {
public:
    TranslationUnit(UniquePtr<SourceFile> file):
        TranslationUnit(toSmallVector(std::move(file))) {}

    TranslationUnit(utl::small_vector<UniquePtr<SourceFile>> files):
        ASTNode(NodeType::TranslationUnit, SourceRange{}, std::move(files)) {
        markDecorated();
    }

    AST_DERIVED_COMMON(TranslationUnit)

    /// List of source files in the translation unit.
    AST_RANGE_PROPERTY(0, SourceFile, sourceFile, SourceFile)
};

/// Concrete node representing a source file
class SCATHA_API SourceFile: public ASTNode {
public:
    SourceFile(std::string filename,
               utl::small_vector<UniquePtr<Statement>> stmts):
        ASTNode(NodeType::SourceFile, SourceRange{}, std::move(stmts)),
        _name(std::move(filename)) {
        markDecorated();
    }

    AST_DERIVED_COMMON(SourceFile)

    /// List of global statements in the translation unit.
    AST_RANGE_PROPERTY(0, Statement, statement, Statement)

    /// \Returns the name of this source file
    std::string const& name() const { return _name; }

private:
    std::string _name;
};

/// Concrete node representing an `import` statement
class SCATHA_API ImportStatement: public Statement {
public:
    explicit ImportStatement(SourceRange sourceRange,
                             UniquePtr<Expression> libExpr,
                             sema::ImportKind importKind):
        Statement(NodeType::ImportStatement, sourceRange, std::move(libExpr)),
        _importKind(importKind) {}

    AST_DERIVED_COMMON(ImportStatement)

    /// The expression denoting the imported library
    AST_PROPERTY(0, Expression, libExpr, LibExpr)

    /// The way to import this library
    sema::ImportKind importKind() const { return _importKind; }

    /// \Returns `importKind() == sema::ImportKind::Scoped`
    bool isScoped() const { return importKind() == sema::ImportKind::Scoped; }

    /// \Returns `importKind() == sema::ImportKind::Unscoped`
    bool isUnscoped() const {
        return importKind() == sema::ImportKind::Unscoped;
    }

    /// \Returns the imported library
    sema::Library* library() {
        return const_cast<sema::Library*>(
            static_cast<ImportStatement const*>(this)->library());
    }

    /// \overload
    sema::Library const* library() const;

private:
    sema::ImportKind _importKind;
};

/// Abstract base class of `VariableDeclaration` and `ParameterDeclaration`
class SCATHA_API VarDeclBase: public Declaration {
public:
    AST_DERIVED_COMMON(VarDeclBase)

    AST_PROPERTY(1, Expression, typeExpr, TypeExpr)

    /// **Decoration provided by semantic analysis**

    /// Declared variable. In most cases this is the object but in some cases
    /// the object is not a variable
    sema::Variable* variable() {
        return const_cast<sema::Variable*>(std::as_const(*this).variable());
    }

    /// \overload
    sema::Variable const* variable() const;

    /// Declared object
    sema::Object* object() {
        return const_cast<sema::Object*>(std::as_const(*this).object());
    }

    /// \overload
    sema::Object const* object() const;

    /// Type of the declaration
    sema::Type const* type() const {
        expectDecorated();
        return _type;
    }

    /// The mutability qualifier of this declaration.
    /// This is set by the parser
    sema::Mutability mutability() const { return _mut; }

    ///
    bool isMut() const { return mutability() == sema::Mutability::Mutable; }

    ///
    bool isConst() const { return !isMut(); }

    /// Decorate this node.
    void decorateVarDecl(sema::Entity* entity);

protected:
    template <typename... Children>
    explicit VarDeclBase(NodeType nodeType, SpecifierList specList,
                         sema::Mutability mut, SourceRange sourceRange,
                         UniquePtr<Identifier> name,
                         UniquePtr<Children>... children):
        Declaration(nodeType, specList, sourceRange, std::move(name),
                    std::move(children)...),
        _mut(mut) {
        if (nameIdentifier()) {
            setSourceRange(nameIdentifier()->sourceRange());
        }
    }

private:
    sema::Type const* _type = nullptr;
    sema::Mutability _mut;
};

/// Concrete node representing a variable declaration.
class SCATHA_API VariableDeclaration: public VarDeclBase {
public:
    explicit VariableDeclaration(SpecifierList specList, sema::Mutability mut,
                                 SourceRange sourceRange,
                                 UniquePtr<Identifier> name,
                                 UniquePtr<Expression> typeExpr,
                                 UniquePtr<Expression> initExpr):
        VarDeclBase(NodeType::VariableDeclaration, specList, mut, sourceRange,
                    std::move(name), std::move(typeExpr), std::move(initExpr)) {
    }

    AST_DERIVED_COMMON(VariableDeclaration)

    /// Expression to initialize this variable. May be null.
    AST_PROPERTY(2, Expression, initExpr, InitExpr)
};

/// Concrete node representing a parameter declaration.
class SCATHA_API ParameterDeclaration: public VarDeclBase {
public:
    explicit ParameterDeclaration(size_t index, sema::Mutability mut,
                                  UniquePtr<Identifier> name,
                                  UniquePtr<Expression> typeExpr):
        ParameterDeclaration(NodeType::ParameterDeclaration, index, mut,
                             SourceRange{}, std::move(name),
                             std::move(typeExpr)) {}

    AST_DERIVED_COMMON(ParameterDeclaration)

    /// The index of this parameter in the parameter list
    size_t index() const { return _index; }

protected:
    explicit ParameterDeclaration(NodeType nodeType, size_t index,
                                  sema::Mutability mut, SourceRange sourceRange,
                                  UniquePtr<Identifier> name,
                                  UniquePtr<Expression> typeExpr):
        VarDeclBase(nodeType, SpecifierList{}, mut, sourceRange,
                    std::move(name), std::move(typeExpr)),
        _index(index) {}

private:
    size_t _index;
};

/// Represents the explicit `this` parameter
class ThisParameter: public ParameterDeclaration {
public:
    explicit ThisParameter(size_t index, sema::Mutability mut, bool isRef,
                           SourceRange sourceRange):
        ParameterDeclaration(NodeType::ThisParameter, index, mut, sourceRange,
                             nullptr, nullptr),
        isRef(isRef) {}

    AST_DERIVED_COMMON(ThisParameter)

    /// The optional reference qualifier attached to the `this` parameter
    bool isReference() const { return isRef; }

private:
    bool isRef;
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
        Statement(NodeType::CompoundStatement, sourceRange,
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
    sema::Scope const* scope() const;

    /// Decorate this node.
    void decorateScope(sema::Scope* scope);
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
        SourceRange sourceRange, SpecifierList specList,
        UniquePtr<Identifier> name,
        utl::small_vector<UniquePtr<ParameterDeclaration>> parameters,
        UniquePtr<Expression> returnTypeExpr,
        UniquePtr<CompoundStatement> body):
        Declaration(NodeType::FunctionDefinition, specList, sourceRange,
                    std::move(name), std::move(returnTypeExpr), std::move(body),
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
    sema::Function* function() {
        return const_cast<sema::Function*>(std::as_const(*this).function());
    }

    /// \overload
    sema::Function const* function() const;

    /// Return type of the function.
    sema::Type const* returnType() const {
        expectDecorated();
        return _returnType;
    }

    /// Decorate this node.
    void decorateFunction(sema::Entity* entity, sema::Type const* returnType) {
        _returnType = returnType;
        Declaration::decorateDecl(entity);
    }

private:
    sema::Type const* _returnType = nullptr;
};

/// Concrete node representing the definition of a struct.
class SCATHA_API StructDefinition: public Declaration {
public:
    explicit StructDefinition(SourceRange sourceRange, SpecifierList specList,
                              UniquePtr<Identifier> name,
                              UniquePtr<CompoundStatement> body):
        Declaration(NodeType::StructDefinition, specList, sourceRange,
                    std::move(name), std::move(body)) {}

    AST_DERIVED_COMMON(StructDefinition)

    /// Body of the struct.
    AST_PROPERTY(1, CompoundStatement, body, Body)

    /// **Decoration provided by semantic analysis**

    /// The struct being defined
    sema::StructType* structType() {
        return const_cast<sema::StructType*>(std::as_const(*this).structType());
    }

    /// \overload
    sema::StructType const* structType() const;
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
    AST_PROPERTY(0, Expression, expression, Expression)
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
        ControlFlowStatement(NodeType::ReturnStatement, sourceRange,
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
        ControlFlowStatement(NodeType::IfStatement, sourceRange,
                             std::move(condition), std::move(ifBlock),
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
    explicit LoopStatement(SourceRange sourceRange, LoopKind kind,
                           UniquePtr<VariableDeclaration> varDecl,
                           UniquePtr<Expression> condition,
                           UniquePtr<Expression> increment,
                           UniquePtr<CompoundStatement> block):
        ControlFlowStatement(NodeType::LoopStatement, sourceRange,
                             std::move(varDecl), std::move(condition),
                             std::move(increment), std::move(block)),
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
    sema::CleanupStack& conditionDtorStack() { return _condDtors; }

    /// \overload
    sema::CleanupStack const& conditionDtorStack() const { return _condDtors; }

    /// The destructor stack of the loop increment expression
    sema::CleanupStack& incrementDtorStack() { return _incDtors; }

    /// \overload
    sema::CleanupStack const& incrementDtorStack() const { return _incDtors; }

private:
    LoopKind _kind;
    sema::CleanupStack _condDtors;
    sema::CleanupStack _incDtors;
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

/// Abstract base class of all conversion nodes
class SCATHA_API ConvExprBase: public Expression {
public:
    AST_DERIVED_COMMON(ConvExprBase)

    /// The expression being converted
    AST_PROPERTY(0, Expression, expression, Expression)

protected:
    explicit ConvExprBase(NodeType nodeType, UniquePtr<Expression> expr):
        Expression(nodeType, expr->sourceRange(), std::move(expr)) {}
};

/// Concrete node representing conversion between value categories
class SCATHA_API ValueCatConvExpr: public ConvExprBase {
public:
    explicit ValueCatConvExpr(UniquePtr<Expression> expr,
                              sema::ValueCatConversion conv):
        ConvExprBase(NodeType::ValueCatConvExpr, std::move(expr)), conv(conv) {}

    AST_DERIVED_COMMON(ValueCatConvExpr)

    /// The kind of conversion
    sema::ValueCatConversion conversion() const { return conv; }

private:
    sema::ValueCatConversion conv;
};

/// Concrete node representing change of mutability
class SCATHA_API MutConvExpr: public ConvExprBase {
public:
    explicit MutConvExpr(UniquePtr<Expression> expr, sema::MutConversion conv):
        ConvExprBase(NodeType::MutConvExpr, std::move(expr)), conv(conv) {}

    AST_DERIVED_COMMON(MutConvExpr)

    /// The kind of conversion
    sema::MutConversion conversion() const { return conv; }

private:
    sema::MutConversion conv;
};

/// Concrete node representing conversion between object types
class SCATHA_API ObjTypeConvExpr: public ConvExprBase {
public:
    explicit ObjTypeConvExpr(UniquePtr<Expression> expr,
                             sema::ObjectTypeConversion conv,
                             sema::ObjectType const* targetType):
        ConvExprBase(NodeType::ObjTypeConvExpr, std::move(expr)),
        conv(conv),
        target(targetType) {}

    AST_DERIVED_COMMON(ObjTypeConvExpr)

    /// The kind of conversion
    sema::ObjectTypeConversion conversion() const { return conv; }

    /// \Returns the object type that we convert to
    sema::ObjectType const* targetType() const { return target; }

private:
    sema::ObjectTypeConversion conv;
    sema::ObjectType const* target;
};

/// Concrete node representing an uninitialized temporary. This node is meant as
/// an object argument to constructor calls
class UninitTemporary: public Expression {
public:
    explicit UninitTemporary(SourceRange sourceRange):
        Expression(NodeType::UninitTemporary, sourceRange) {}

    AST_DERIVED_COMMON(UninitTemporary)
};

/// ...
class SCATHA_API ConstructExpr: public CallLike {
public:
    /// Construct object from arbitrary arguments
    explicit ConstructExpr(utl::small_vector<UniquePtr<Expression>> arguments,
                           sema::ObjectType const* constructedType,
                           SourceRange sourceRange):
        CallLike(NodeType::ConstructExpr, nullptr, std::move(arguments),
                 sourceRange),
        constrType(constructedType) {}

    /// Default construct object
    explicit ConstructExpr(sema::ObjectType const* constructedType,
                           SourceRange sourceRange):
        ConstructExpr({}, constructedType, sourceRange) {}

    /// Copy construct object
    explicit ConstructExpr(UniquePtr<Expression> argument):
        ConstructExpr(std::move(argument), argument.get()) {}

    AST_DERIVED_COMMON(ConstructExpr)

    /// Decorates this `ConstructExpr`
    void decorateConstruct(sema::Entity* entity, sema::Function* function) {
        _function = function;
        decorateValue(entity, sema::ValueCategory::RValue);
    }

    /// The type being constructed.
    sema::ObjectType const* constructedType() const { return constrType; }

    /// \Returns `true` if the constructed type does not have trivial lifetime
    bool isTrivial() const { return !function(); }

    /// The lifetime function to call. This is only non-null if the contructed
    /// type does have trivial lifetime
    sema::Function* function() const { return _function; }

private:
    /// Helper for "Copy object"
    template <typename R>
    explicit ConstructExpr(R&& rangeArg, auto* arg):
        ConstructExpr(toSmallVector(std::forward<R>(rangeArg)),
                      arg->type().get(), arg->sourceRange()) {}

    sema::ObjectType const* constrType;
    sema::Function* _function;
};

/// Abstract base class of the `*ConstructExpr` classes
class SCATHA_API ConstructBase: public CallLike {
public:
    /// The expression denoting the type that is being constructed
    /// This is not necessarily non-null even for correct programs
    AST_PROPERTY(0, Expression, targetTypeExpr, TargetTypeExpr)

    /// The type being constructed. If `targetTypeExpr()` is non-null it denotes
    /// this type
    sema::ObjectType const* constructedType() const { return constrType; }

protected:
    ConstructBase(NodeType nodeType, UniquePtr<Expression> typeExpr,
                  utl::small_vector<UniquePtr<Expression>> arguments,
                  SourceRange sourceRange,
                  sema::ObjectType const* constructedType):
        CallLike(nodeType, std::move(typeExpr), std::move(arguments),
                 sourceRange),
        constrType(constructedType) {}

    ///
    void decorateConstruct(sema::Entity* entity) {
        decorateValue(entity, sema::ValueCategory::RValue);
    }

private:
    sema::ObjectType const* constrType;
};

/// Concrete node that is only inserted by semantic analysis.
/// Represents an expression like `Foo()` or the initializer of a variable
/// declaration `var f: Foo;` where `Foo` is trivially default constructible
class SCATHA_API TrivDefConstructExpr: public ConstructBase {
public:
    ///
    explicit TrivDefConstructExpr(SourceRange sourceRange,
                                  sema::ObjectType const* constructedType):
        ConstructBase(NodeType::TrivDefConstructExpr, nullptr, {}, sourceRange,
                      constructedType) {}

    /// All trivial construct expressions inherit this from `ConstructBase`
    using ConstructBase::decorateConstruct;
};

/// Concrete node that is only inserted by semantic analysis.
/// Represents the initializer of a variable declaration `var g = f;` where `f`
/// is of trivially copyable type
class SCATHA_API TrivCopyConstructExpr: public ConstructBase {
public:
    ///
    explicit TrivCopyConstructExpr(UniquePtr<Expression> argument,
                                   SourceRange sourceRange,
                                   sema::ObjectType const* constructedType):
        ConstructBase(NodeType::TrivCopyConstructExpr, nullptr,
                      toSmallVector(std::move(argument)), sourceRange,
                      constructedType) {}

    /// The expression denoting the value being copied
    AST_PROPERTY(1, Expression, argument, Argument)

    /// All trivial construct expressions inherit this from `ConstructBase`
    using ConstructBase::decorateConstruct;
};

/// Concrete node that is only inserted by semantic analysis.
/// Represents non-default aggregate construction, i.e. an expression like
/// `Foo(1, 2.0, true)` where `Foo` has data members of type `int`, `double` and
/// `bool` in this order and is trivially constructible
class SCATHA_API TrivAggrConstructExpr: public ConstructBase {
public:
    explicit TrivAggrConstructExpr(
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange, sema::ObjectType const* constructedType):
        ConstructBase(NodeType::TrivAggrConstructExpr, nullptr,
                      std::move(arguments), sourceRange, constructedType) {}

    /// All trivial construct expressions inherit this from `ConstructBase`
    using ConstructBase::decorateConstruct;
};

/// Concrete node that is only inserted by semantic analysis.
/// Represents non-default non-trivial aggregate construction, i.e. an
/// expression like `Foo(Bar(), 2.0, true)` where `Foo` has data members of
/// non-trivial lifetime
class SCATHA_API NontrivAggrConstructExpr: public ConstructBase {
public:
    explicit NontrivAggrConstructExpr(
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange, sema::StructType const* constructedType);

    /// \Returns the being constructed
    sema::StructType const* constructedType() const;

    /// All trivial construct expressions inherit this from `ConstructBase`
    using ConstructBase::decorateConstruct;
};

/// Represents nontrivial object construction of struct types. Other types like
/// arrays and unique pointers are constructed by `NontrivInlineConstructExpr`
class SCATHA_API NontrivConstructExpr: public ConstructBase {
public:
    explicit NontrivConstructExpr(
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange, sema::StructType const* constructedType);

    /// \Returns the being constructed
    sema::StructType const* constructedType() const;

    /// \Returns the constructor selected by semantic analysis
    sema::Function const* constructor() const { return ctor; }

    ///
    void decorateConstruct(sema::Object* obj,
                           sema::Function const* constructor);

private:
    sema::Function const* ctor = nullptr;
};

/// Represents nontrivial object construction of array and unique pointer types
class SCATHA_API NontrivInlineConstructExpr: public ConstructBase {
public:
    explicit NontrivInlineConstructExpr(
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange, sema::ObjectType const* constructedType):
        ConstructBase(NodeType::NontrivInlineConstructExpr, nullptr,
                      std::move(arguments), sourceRange, constructedType) {}

    using ConstructBase::decorateConstruct;

    ///
    sema::SMFKind operation() const { return op; }

    ///
    void decorateConstruct(sema::Entity* entity, sema::SMFKind operation) {
        ConstructBase::decorateConstruct(entity);
        op = operation;
    }

private:
    sema::SMFKind op{};
};

/// Represents construction of dynamic array
class SCATHA_API DynArrayConstructExpr: public ConstructBase {
public:
    explicit DynArrayConstructExpr(
        utl::small_vector<UniquePtr<Expression>> arguments,
        SourceRange sourceRange, sema::ArrayType const* constructedType);

    /// \Returns the being constructed
    sema::ArrayType const* constructedType() const;

    using ConstructBase::decorateConstruct;

    /// The expression that is executed in a loop to initialize the elements
    /// \Warning Here we reuse index zero that `CallLike` already uses for the
    /// 'callee'. We hope this will be fine
    AST_PROPERTY(0, Expression, elementConstruction, ElementConstruction)
};

/// Concrete node representing an assignment of a non-trivial value
/// We need this node to tell the IR generator to invoke the destructor of the
/// value before calling the copy constructor again
class SCATHA_API NontrivAssignExpr: public Expression {
public:
    explicit NontrivAssignExpr(UniquePtr<Expression> dest,
                               UniquePtr<Expression> source):
        Expression(NodeType::NontrivAssignExpr,
                   merge(source->sourceRange(), dest->sourceRange()),
                   std::move(dest), std::move(source)) {}

    AST_DERIVED_COMMON(NontrivAssignExpr)

    /// The object that is assigned to
    AST_PROPERTY(0, Expression, dest, Dest)

    /// The object that is assigned from
    AST_PROPERTY(1, Expression, source, Source)

    /// **Decoration provided by semantic analysis**

    /// The copy operation to be performed on assignment
    sema::SMFKind copyOperation() const { return copyOp; }

    /// Note to the IR generator
    bool mustCheckForSelfAssignment() const { return mustCheckSelfAssign; }

    /// Decorate this node
    void decorateAssign(sema::Entity* entity, sema::SMFKind copyOp,
                        bool mustCheckSelfAssign) {
        decorateValue(entity, sema::ValueCategory::RValue);
        this->copyOp = copyOp;
        this->mustCheckSelfAssign = mustCheckSelfAssign;
    }

private:
    sema::SMFKind copyOp;
    bool mustCheckSelfAssign = false;
};

} // namespace scatha::ast

#undef AST_DERIVED_COMMON
#undef AST_PROPERTY
#undef AST_RANGE_PROPERTY

#endif // SCATHA_AST_AST_H_
