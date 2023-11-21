#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using enum sema::Mutability;

TEST_CASE("SymbolTable lookup", "[sema]") {
    sema::SymbolTable sym;
    auto* var = sym.defineVariable("x", sym.S64(), Mutable);
    CHECK(var == sym.unqualifiedLookup("x").front());
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    /// Declare function `i` in the global scope
    auto* fnI =
        sym.declareFunction("i",
                            sema::FunctionSignature({ sym.S64() }, sym.S64()));
    REQUIRE(fnI);
    auto* xType = sym.declareStructureType("X");
    REQUIRE(xType);
    auto* memberI = sym.withScopePushed(xType, [&] {
        return sym.defineVariable("i", sym.S64(), Mutable);
    });
    xType->setSize(8);
    auto overloadSet = sym.unqualifiedLookup("i");
    REQUIRE(!overloadSet.empty());
    sym.pushScope(xType);
    auto* memberVar = sym.unqualifiedLookup("i").front();
    CHECK(memberI == memberVar);
    sym.popScope();
}
