#include "Sema/Fwd.h"

using namespace scatha;
using namespace sema;

std::string_view sema::toString(EntityType t) {
    return std::array{
#define SC_SEMA_ENTITY_DEF(Type, _) std::string_view(#Type),
#include "Sema/Lists.def"
    }[static_cast<size_t>(t)];
}

std::ostream& sema::operator<<(std::ostream& str, EntityType t) {
    return str << toString(t);
}

std::ostream& sema::operator<<(std::ostream& str, EntityCategory cat) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(cat, {
        { EntityCategory::Indeterminate, "Indeterminate" },
        { EntityCategory::Value,         "Value" },
        { EntityCategory::Type,          "Type" },
    });
    // clang-format on
}

EntityCategory sema::categorize(EntityType entityType) {
    // clang-format off
    return UTL_MAP_ENUM(entityType, EntityCategory, {
        { EntityType::Entity,         EntityCategory::Indeterminate },
        { EntityType::Scope,          EntityCategory::Indeterminate },
        { EntityType::GlobalScope,    EntityCategory::Indeterminate },
        { EntityType::Function,       EntityCategory::Value },
        { EntityType::Type,           EntityCategory::Type },
        { EntityType::ObjectType,     EntityCategory::Type },
        { EntityType::StructureType,  EntityCategory::Type },
        { EntityType::ArrayType,      EntityCategory::Type },
        { EntityType::QualType,       EntityCategory::Type },
        { EntityType::AnonymousScope, EntityCategory::Indeterminate },
        { EntityType::OverloadSet,    EntityCategory::Value },
        { EntityType::Variable,       EntityCategory::Value },
    }); // clang-format on
}

std::ostream& sema::operator<<(std::ostream& str, ValueCategory cat) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(cat, {
        { ValueCategory::None,   "None" },
        { ValueCategory::LValue, "LValue" },
        { ValueCategory::RValue, "RValue" },
    });
    // clang-format on
}

std::string_view sema::toString(ScopeKind k) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(k, {
        { ScopeKind::Invalid,   "Invalid" },
        { ScopeKind::Global,    "Global" },
        { ScopeKind::Namespace, "Namespace" },
        { ScopeKind::Variable,  "Variable" },
        { ScopeKind::Function,  "Function" },
        { ScopeKind::Object,    "Object" },
        { ScopeKind::Anonymous, "Anonymous" },
    }); // clang-format on
}

std::ostream& sema::operator<<(std::ostream& str, ScopeKind k) {
    return str << toString(k);
}
