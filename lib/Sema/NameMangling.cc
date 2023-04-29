#include "Sema/NameMangling.h"

#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

static std::string baseImpl(Entity const* entity) {
    std::string result = std::string(entity->name());
    Scope const* scope = entity->parent();
    while (!isa<GlobalScope>(scope)) {
        result = utl::strcat(scope->name(), ".", result);
        scope  = scope->parent();
    }
    return result;
}

static std::string impl(Entity const* entity) { SC_DEBUGFAIL(); }

static std::string impl(ObjectType const* type) { return baseImpl(type); }

static std::string impl(QualType const* type) {
    std::string const baseName = impl(type->base());
    std::stringstream sstr;
    if (type->isReference()) {
        sstr << "&";
    }
    if (type->isMutable()) {
        sstr << "mut";
    }
    if (type->isArray()) {
        sstr << "[" << baseName << "]";
    }
    else {
        sstr << baseName;
    }
    return std::move(sstr).str();
}

static std::string impl(Function const* func) {
    std::stringstream sstr;
    sstr << baseImpl(func) << "(";
    for (auto* arg: func->signature().argumentTypes()) {
        sstr << mangleName(arg);
    }
    sstr << ")";
    return std::move(sstr).str();
}

std::string sema::mangleName(Entity const* entity) {
    return visit(*entity, [](auto& entity) { return impl(&entity); });
}