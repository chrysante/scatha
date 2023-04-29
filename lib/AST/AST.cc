#include "AST/AST.h"

#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;
using namespace sema;

void scatha::internal::privateDelete(AbstractSyntaxTree* node) {
    visit(*node, [](auto& derived) { delete &derived; });
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

sema::ObjectType const* Expression::typeBaseOrTypeEntity() const {
    return isValue() ? type()->base() : cast<ObjectType const*>(entity());
}
