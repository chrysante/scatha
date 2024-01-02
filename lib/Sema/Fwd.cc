#include "Sema/Fwd.h"

#include <ostream>

#include <utl/hashtable.hpp>

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
#include <scatha/Sema/Lists.def>
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

std::string_view sema::toString(SpecialMemberFunction SMF) {
    return std::array{
#define SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(name, str) std::string_view(str),
#include "Sema/Lists.def"
    }[static_cast<size_t>(SMF)];
}

std::ostream& sema::operator<<(std::ostream& str, SpecialMemberFunction SMF) {
    return str << toString(SMF);
}

SpecialMemberFunction sema::toSMF(SpecialLifetimeFunction SLF) {
    using enum SpecialLifetimeFunction;
    using enum SpecialMemberFunction;
    switch (SLF) {
    case DefaultConstructor:
        return New;
    case CopyConstructor:
        return New;
    case MoveConstructor:
        return Move;
    case Destructor:
        return Delete;
    }
    SC_UNREACHABLE();
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
