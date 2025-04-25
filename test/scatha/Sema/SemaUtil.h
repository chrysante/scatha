#ifndef SCATHA_SEMA_SEMAUTIL_H_
#define SCATHA_SEMA_SEMAUTIL_H_

#include <stdexcept>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

namespace scatha::test {

template <typename T>
static T* entityAs(auto* entity) {
    auto* t = dyncast<T*>(entity);
    if (!t) {
        throw std::runtime_error("Invalid type");
    }
    return t;
}

/// \Returns the first found entity
/// \Throws if none is found
template <typename T = sema::Entity>
static T* find(sema::Scope* scope, std::string_view name) {
    auto entities = scope->findEntities(name);
    REQUIRE(entities.size() == 1);
    return entityAs<T>(entities.front());
}

/// \Returns the first found entity
/// \Throws if none is found
template <typename T = sema::Entity>
static T* lookup(sema::SymbolTable& sym, std::string_view name) {
    auto entities = sym.unqualifiedLookup(name);
    REQUIRE(entities.size() == 1);
    return entityAs<T>(sema::stripAlias(entities.front()));
}

///
struct Finder {
    sema::SymbolTable const& sym;

    sema::Entity const* findImpl(std::string_view name) const {
        auto entities = sym.currentScope().findEntities(name);
        if (!entities.empty()) {
            return sema::stripAlias(entities.front());
        }
        return nullptr;
    }

    sema::Scope const* operator()(std::string_view name, auto fn) const {
        auto* entity = findImpl(name);
        if (!entity) {
            throw std::runtime_error("Failed to find \"" + std::string(name) +
                                     "\"");
        }
        auto* scope = dyncast<sema::Scope const*>(entity);
        if (!scope) {
            throw std::runtime_error("\"" + std::string(name) +
                                     "\" is not a scope");
        }
        sym.withScopePushed(scope, [&] { fn(scope); });
        return scope;
    }

    sema::Entity const* operator()(std::string_view name) const {
        return findImpl(name);
    }
};

} // namespace scatha::test

#endif // SCATHA_SEMA_SEMAUTIL_H_
