#ifndef TEST_SEMAUTIL_H_
#define TEST_SEMAUTIL_H_

#include <stdexcept>
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

} // namespace scatha::test

#endif // TEST_SEMAUTIL_H_
