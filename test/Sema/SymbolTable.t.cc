#include <catch2/catch_test_macros.hpp>

#include <array>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum sema::Mutability;

TEST_CASE("SymbolTable lookup", "[sema]") {
    sema::SymbolTable sym;
    auto* var =
        sym.defineVariable("x", sym.S64(), Mutable, AccessControl::Public);
    CHECK(var == sym.unqualifiedLookup("x").front());
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    /// Declare function `i` in the global scope
    auto* fnI = sym.declareFunction("i",
                                    sym.functionType({ sym.S64() }, sym.S64()),
                                    AccessControl::Public);
    REQUIRE(fnI);
    auto* xType = sym.declareStructureType("X", AccessControl::Public);
    REQUIRE(xType);
    auto* memberI = sym.withScopePushed(xType, [&] {
        return sym.defineVariable("i",
                                  sym.S64(),
                                  Mutable,
                                  AccessControl::Public);
    });
    xType->setSize(8);
    auto overloadSet = sym.unqualifiedLookup("i");
    REQUIRE(!overloadSet.empty());
    sym.pushScope(xType);
    auto* memberVar = sym.unqualifiedLookup("i").front();
    CHECK(memberI == memberVar);
    sym.popScope();
}
