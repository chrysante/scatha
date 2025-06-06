#include "Sema/Fwd.h"

#include <ostream>
#include <string>

#include <utl/hashtable.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

std::string_view sema::toString(EntityType t) {
    return std::array{
#define SC_SEMA_ENTITY_DEF(Type, ...) std::string_view(#Type),
#include "Sema/Lists.def.h"
    }[static_cast<size_t>(t)];
}

std::ostream& sema::operator<<(std::ostream& str, EntityType t) {
    return str << toString(t);
}

std::ostream& sema::operator<<(std::ostream& str, EntityCategory cat) {
    switch (cat) {
    case EntityCategory::Indeterminate:
        return str << "Indeterminate";
    case EntityCategory::Value:
        return str << "Value";
    case EntityCategory::Type:
        return str << "Type";
    case EntityCategory::Namespace:
        return str << "Namespace";
    }
    SC_UNREACHABLE();
}

std::string_view sema::toString(ValueCategory cat) {
    using enum ValueCategory;
    switch (cat) {
    case LValue:
        return "lvalue";
    case RValue:
        return "rvalue";
    }
    SC_UNREACHABLE();
}

std::ostream& sema::operator<<(std::ostream& str, ValueCategory cat) {
    return str << toString(cat);
}

ValueCategory sema::commonValueCat(ValueCategory a, ValueCategory b) {
    if (a == b) {
        return a;
    }
    return ValueCategory::RValue;
}

std::string_view sema::toString(Mutability mut) {
    using enum Mutability;
    switch (mut) {
    case Mutable:
        return "mutable";
    case Const:
        return "immutable";
    }
    SC_UNREACHABLE();
}

std::ostream& sema::operator<<(std::ostream& str, Mutability mut) {
    return str << toString(mut);
}

std::string_view sema::toString(PointerBindMode mode) {
    using enum PointerBindMode;
    switch (mode) {
    case Static:
        return "static";
    case Dynamic:
        return "dynamic";
    }
    SC_UNREACHABLE();
}

std::ostream& sema::operator<<(std::ostream& str, PointerBindMode mode) {
    return str << toString(mode);
}

std::string_view sema::toString(AccessControl a) {
    return std::array{
#define SC_SEMA_ACCESS_CONTROL_DEF(Kind, Spelling) std::string_view(Spelling),
#include "Sema/Lists.def.h"
    }[static_cast<size_t>(a)];
}

std::ostream& sema::operator<<(std::ostream& ostream,
                               AccessControl accessControl) {
    return ostream << toString(accessControl);
}

std::string_view sema::toString(ScopeKind kind) {
    switch (kind) {
    case ScopeKind::Invalid:
        return "Invalid";
    case ScopeKind::Global:
        return "Global";
    case ScopeKind::Namespace:
        return "Namespace";
    case ScopeKind::Function:
        return "Function";
    case ScopeKind::Type:
        return "Type";
    }
    SC_UNREACHABLE();
}

std::ostream& sema::operator<<(std::ostream& str, ScopeKind k) {
    return str << toString(k);
}

std::string_view sema::toString(PropertyKind kind) {
    using enum PropertyKind;
    switch (kind) {
#define SC_SEMA_PROPERTY_KIND(K, SourceName)                                   \
    case K:                                                                    \
        return SourceName;
#include <scatha/Sema/Lists.def.h>
    }
    SC_UNREACHABLE();
}

std::ostream& sema::operator<<(std::ostream& str, PropertyKind kind) {
    return str << toString(kind);
}

std::string_view sema::toString(FunctionKind k) {
    switch (k) {
    case FunctionKind::Native:
        return "Native";
    case FunctionKind::Foreign:
        return "Foreign";
    case FunctionKind::Generated:
        return "Generated";
    }
    SC_UNREACHABLE();
}

std::ostream& sema::operator<<(std::ostream& str, FunctionKind k) {
    return str << toString(k);
}

std::string sema::toString(SMFKind kind) {
    return std::string(std::array{
#define SC_SEMA_SMF_DEF(Name, Spelling) std::string_view(#Name),
#include "Sema/Lists.def.h"
    }[(size_t)kind]);
}

std::ostream& sema::operator<<(std::ostream& ostream, SMFKind kind) {
    return ostream << toString(kind);
}

std::string sema::toSpelling(SMFKind kind) {
    return std::array{
#define SC_SEMA_SMF_DEF(Name, Spelling) Spelling,
#include "Sema/Lists.def.h"
    }[(size_t)kind];
}

std::string_view sema::toString(ConstantKind k) {
    return std::array{
#define SC_SEMA_CONSTKIND_DEF(Kind, ...) std::string_view(#Kind),
#include "Sema/Lists.def.h"
    }[static_cast<size_t>(k)];
}

std::ostream& sema::operator<<(std::ostream& str, ConstantKind k) {
    return str << toString(k);
}

std::string_view sema::toString(ValueCatConversion conv) {
    return std::array{
#define SC_VALUECATCONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Conversion.def.h"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream, ValueCatConversion conv) {
    return ostream << toString(conv);
}

std::string_view sema::toString(QualConversion conv) {
    return std::array{
#define SC_QUALCONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Conversion.def.h"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream, QualConversion conv) {
    return ostream << toString(conv);
}

std::string_view sema::toString(ObjectTypeConversion conv) {
    return std::array{
#define SC_OBJTYPECONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Conversion.def.h"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream,
                               ObjectTypeConversion conv) {
    return ostream << toString(conv);
}

bool sema::isConstruction(ObjectTypeConversion conv) {
    using enum ObjectTypeConversion;
    return (int)conv >= (int)TrivDefConstruct &&
           (int)conv <= (int)DynArrayConstruct;
}

bool sema::isArithmeticConversion(ObjectTypeConversion conv) {
    switch (conv) {
#define SC_ARITHMETIC_CONV_DEF(CONV, ...)                                      \
    case ObjectTypeConversion::CONV:                                           \
        return true;
#include "Sema/Conversion.def.h"
    default:
        return false;
    }
}
