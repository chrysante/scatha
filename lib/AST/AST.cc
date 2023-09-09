#include "AST/AST.h"

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void ast::privateDelete(ASTNode* node) {
    visit(*node, [](auto& derived) { delete &derived; });
}

static void extSourceRangeImpl(SourceRange& result, ASTNode const* node) {
    result = merge(result, node->sourceRange());
    for (auto* child: node->children()) {
        if (child) {
            extSourceRangeImpl(result, child);
        }
    }
}

SourceRange ASTNode::extSourceRange() const {
    SourceRange result;
    extSourceRangeImpl(result, this);
    return result;
}

UniquePtr<ASTNode> ASTNode::extractFromParent() {
    return parent()->extractChild(indexInParent());
}

void ASTNode::replaceChild(ASTNode const* old, UniquePtr<ASTNode> repl) {
    auto itr = ranges::find_if(_children,
                               [&](auto& child) { return child.get() == old; });
    SC_ASSERT(itr != ranges::end(_children), "`old` is not a child of this");
    repl->_parent = this;
    *itr = std::move(repl);
}

size_t ASTNode::indexOf(ASTNode const* child) const {
    auto itr = ranges::find_if(_children, [&](auto& otherChild) {
        return otherChild.get() == child;
    });
    return utl::narrow_cast<size_t>(itr - _children.begin());
}

sema::QualType Expression::typeOrTypeEntity() const {
    return isValue() ? type() : cast<sema::ObjectType const*>(entity());
}

/// Convenience wrapper for `isa<sema::ReferenceType>(type());`
bool Expression::isLValueNEW() const {
    SC_ASSERT(isValue(), "Must be a value to be an LValue");
    return isa<sema::ReferenceType>(*type());
}

/// Convenience wrapper for `!isa<sema::ReferenceType>(type());`
bool Expression::isRValueNEW() const {
    SC_ASSERT(isValue(), "Must be a value to be an RValue");
    return !isa<sema::ReferenceType>(*type());
}

void Expression::decorateExpr(sema::Entity* entity,
                              sema::QualType type,
                              std::optional<sema::EntityCategory> entityCat) {
    _entity = entity;
    _type = type;
    /// Derive defaults
    if (entity) {
        _entityCat = entity->category();
    }
    else {
        _entityCat = sema::EntityCategory::Value;
    }
    auto* object = dyncast_or_null<sema::Object*>(entity);
    if (!type && object) {
        _type = object->type();
    }
    /// Override if user specified
    if (entityCat) {
        _entityCat = *entityCat;
    }
    markDecorated();
}

void FunctionCall::decorateCall(sema::Object* object,
                                sema::QualType type,
                                sema::Function* calledFunction) {
    decorateExpr(object, type);
    _function = calledFunction;
}

Conversion::Conversion(UniquePtr<Expression> expr,
                       std::unique_ptr<sema::Conversion> conv):
    Expression(NodeType::Conversion, expr->sourceRange(), std::move(expr)),
    _conv(std::move(conv)) {}

Conversion::~Conversion() = default;

sema::QualType Conversion::targetType() const {
    return conversion()->targetType();
}

sema::ObjectType const* ConstructorCall::constructedType() const {
    return cast<sema::ObjectType const*>(function()->parent());
}
