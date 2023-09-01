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

bool sema::isRef(QualType type) { return isa<ReferenceType>(*type); }

bool sema::isImplRef(QualType type) {
    return isRef(type) && cast<ReferenceType const&>(*type).isImplicit();
}

bool sema::isExplRef(QualType type) {
    return isRef(type) && cast<ReferenceType const&>(*type).isExplicit();
}

std::optional<Reference> sema::refKind(QualType type) {
    if (auto* refType = dyncast<ReferenceType const*>(type.get())) {
        return refType->reference();
    }
    return std::nullopt;
}

QualType sema::stripReference(QualType type) {
    if (auto* refType = dyncast<ReferenceType const*>(type.get())) {
        return refType->base();
    }
    return type;
}

QualType sema::stripQualifiers(QualType type) {
    return stripReference(type).toMut();
}

std::string_view sema::toString(SpecialMemberFunction SMF) {
    return std::array{
#define SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(name, str) std::string_view(str),
#include "Sema/Lists.def"
    }[static_cast<size_t>(SMF)];
}

std::ostream& sema::operator<<(std::ostream& str, SpecialMemberFunction SMF) {
    return str << toString(SMF);
}

std::string_view sema::toString(ConstantKind k) {
    return std::array{
#define SC_SEMA_CONSTKIND_DEF(Kind, _) std::string_view(#Kind),
#include "Sema/Lists.def"
    }[static_cast<size_t>(k)];
}

std::ostream& sema::operator<<(std::ostream& str, ConstantKind k) {
    return str << toString(k);
}
