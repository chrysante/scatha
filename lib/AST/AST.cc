#include "AST/AST.h"

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void scatha::internal::privateDelete(AbstractSyntaxTree* node) {
    visit(*node, [](auto& derived) { delete &derived; });
}

UniquePtr<AbstractSyntaxTree> AbstractSyntaxTree::extractFromParent() {
    return parent()->extractChild(indexInParent());
}

void AbstractSyntaxTree::replaceChild(AbstractSyntaxTree const* old,
                                      UniquePtr<AbstractSyntaxTree> repl) {
    auto itr = ranges::find_if(_children,
                               [&](auto& child) { return child.get() == old; });
    SC_ASSERT(itr != ranges::end(_children), "`old` is not a child of this");
    *itr = std::move(repl);
}

size_t AbstractSyntaxTree::indexOf(AbstractSyntaxTree const* child) const {
    auto itr = ranges::find_if(_children, [&](auto& otherChild) {
        return otherChild.get() == child;
    });
    return utl::narrow_cast<size_t>(itr - _children.begin());
}

void Expression::decorate(sema::Entity* entity,
                          sema::QualType const* type,
                          std::optional<sema::ValueCategory> valueCat,
                          std::optional<sema::EntityCategory> entityCat) {
    _entity = entity;
    _type   = type;
    /// Derive defaults
    if (entity) {
        _valueCat  = entity->isValue() ? sema::ValueCategory::LValue :
                                         sema::ValueCategory::None;
        _entityCat = entity->category();
    }
    else {
        _valueCat  = sema::ValueCategory::RValue;
        _entityCat = sema::EntityCategory::Value;
    }
    /// Override if user specified
    if (valueCat) {
        _valueCat = *valueCat;
    }
    if (entityCat) {
        _entityCat = *entityCat;
    }
    markDecorated();
}

sema::QualType const* Expression::typeOrTypeEntity() const {
    return isValue() ? type() : cast<sema::QualType const*>(entity());
}

Conversion::Conversion(UniquePtr<Expression> expr,
                       std::unique_ptr<sema::Conversion> conv):
    Expression(NodeType::Conversion, expr->sourceRange(), std::move(expr)),
    _conv(std::move(conv)) {}

Conversion::~Conversion() = default;

sema::QualType const* Conversion::targetType() const {
    return conversion()->targetType();
}
