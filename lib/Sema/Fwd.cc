#include "Sema/Fwd.h"

#include <ostream>

#include "Sema/Entity.h"

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
        { EntityType::BuiltinType,    EntityCategory::Type },
        { EntityType::VoidType,       EntityCategory::Type },
        { EntityType::BoolType,       EntityCategory::Type },
        { EntityType::ByteType,       EntityCategory::Type },
        { EntityType::ArithmeticType, EntityCategory::Type },
        { EntityType::IntType,        EntityCategory::Type },
        { EntityType::FloatType,      EntityCategory::Type },
        { EntityType::StructureType,  EntityCategory::Type },
        { EntityType::ArrayType,      EntityCategory::Type },
        { EntityType::QualType,       EntityCategory::Type },
        { EntityType::AnonymousScope, EntityCategory::Indeterminate },
        { EntityType::OverloadSet,    EntityCategory::Value },
        { EntityType::Generic,        EntityCategory::Indeterminate },
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

std::string_view sema::toString(FunctionKind k) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(k, {
        { FunctionKind::Native,   "Native" },
        { FunctionKind::External, "External" },
        { FunctionKind::Special,  "Special" }
    }); // clang-format on
}

std::ostream& sema::operator<<(std::ostream& str, FunctionKind k) {
    return str << toString(k);
}

std::string_view sema::toString(ConversionKind k) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(k, {
        { ConversionKind::SignedToUnsigned, "SignedToUnsigned" },
        { ConversionKind::UnsignedToSigned, "UnsignedToSigned" },
    }); // clang-format on
}

std::ostream& sema::operator<<(std::ostream& str, ConversionKind k) {
    return str << toString(k);
}

Mutability sema::baseMutability(QualType const* type) {
    if (type->isMutRef()) {
        return Mutability::Mutable;
    }
    return type->mutability();
}

Reference sema::toExplicitRef(Mutability mut) {
    switch (mut) {
    case Mutability::Mutable:
        return RefMutExpl;
    case Mutability::Const:
        return RefConstExpl;
    }
}

Reference sema::toImplicitRef(Mutability mut) {
    switch (mut) {
    case Mutability::Mutable:
        return RefMutImpl;
    case Mutability::Const:
        return RefConstImpl;
    }
}
