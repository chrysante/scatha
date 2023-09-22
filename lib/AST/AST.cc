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

SourceRange ASTNode::sourceRange() const {
    SourceRange result = directSourceRange();
    for (auto* child: children()) {
        if (child && !isa<CompoundStatement>(child)) {
            result = merge(result, child->sourceRange());
        }
    }
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

sema::Object const* Expression::object() const {
    return cast<sema::Object const*>(entity());
}

sema::EntityCategory Expression::entityCategory() const {
    expectDecorated();
    if (!entity()) {
        return sema::EntityCategory::Indeterminate;
    }
    return entity()->category();
}

void Expression::decorateValue(sema::Entity* entity,
                               sema::ValueCategory valueCategory,
                               sema::QualType type) {
    SC_ASSERT(entity, "Must not be null");
    _entity = entity;
    _valueCat = valueCategory;
    _type = type;
    auto* object = dyncast_or_null<sema::Object*>(entity);
    if (!type && object) {
        _type = object->getQualType();
    }
    markDecorated();
}

void Expression::decorateType(sema::Type* type) {
    _entity = type;
    markDecorated();
}

void FunctionCall::decorateCall(sema::Object* object,
                                sema::ValueCategory valueCategory,
                                sema::QualType type,
                                sema::Function* calledFunction) {
    decorateValue(object, valueCategory, type);
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

void VarDeclBase::decorateVarDecl(sema::Entity* entity) {
    if (auto* object = dyncast_or_null<sema::Object*>(entity)) {
        _type = object->type();
    }
    Declaration::decorateDecl(entity);
}

sema::Variable const* VarDeclBase::variable() const {
    return cast<sema::Variable const*>(entity());
}

sema::Function const* FunctionDefinition::function() const {
    return cast<sema::Function const*>(entity());
}

sema::StructType const* StructDefinition::structType() const {
    return cast<sema::StructType const*>(entity());
}
