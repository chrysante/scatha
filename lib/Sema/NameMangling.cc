#include "Sema/NameMangling.h"

#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Entity.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace sema;

static std::string baseImpl(Entity const* entity) {
    std::string result = std::string(entity->name());
    Scope const* scope = entity->parent();
    SC_EXPECT(scope);
    while (true) {
        if (scope->kind() == ScopeKind::Global) {
            break;
        }
        result = utl::strcat(scope->name(), ".", result);
        scope = scope->parent();
        SC_EXPECT(scope);
    }
    return result;
}

static std::string impl(QualType type) {
    if (type.isMut()) {
        return utl::strcat("mut-", type->mangledName());
    }
    return type->mangledName();
}

static std::string impl(Entity const*) { SC_UNREACHABLE(); }

static std::string impl(ObjectType const* type) { return baseImpl(type); }

static std::string impl(ArrayType const* type) {
    return utl::strcat("[", impl(type->elementType()), "]");
}

static std::string impl(RawPtrType const* type) {
    return utl::strcat("*", impl(type->base()));
}

static std::string impl(ReferenceType const* type) {
    return utl::strcat("&", impl(type->base()));
}

static std::string impl(UniquePtrType const* type) {
    return utl::strcat("*unique-", impl(type->base()));
}

static std::string impl(Function const* func) {
    std::stringstream sstr;
    sstr << baseImpl(func);
    for (auto* arg: func->signature().argumentTypes()) {
        sstr << "-" << mangleName(arg);
    }
    return std::move(sstr).str();
}

std::string sema::mangleName(Entity const* entity) {
    return visit(*entity, [](auto& entity) { return impl(&entity); });
}
