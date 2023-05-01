#include "AST/AST.h"

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;
using namespace sema;

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

void Expression::decorate(Entity* entity,
                          QualType const* type,
                          std::optional<ValueCategory> valueCat,
                          std::optional<EntityCategory> entityCat) {
    _entity = entity;
    _type   = type;
    /// Derive defaults
    if (entity) {
        _valueCat =
            entity->isValue() ? ValueCategory::LValue : ValueCategory::None;
        _entityCat = entity->category();
    }
    else {
        _valueCat  = ValueCategory::RValue;
        _entityCat = EntityCategory::Value;
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
    return isValue() ? type() : cast<QualType const*>(entity());
}
