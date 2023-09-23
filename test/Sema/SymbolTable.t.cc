#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using enum sema::Mutability;

TEST_CASE("SymbolTable lookup", "[sema]") {
    sema::SymbolTable sym;
    auto* var = sym.defineVariable("x", sym.S64(), Mutable);
    CHECK(var == sym.lookup("x"));
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    /// Declare function `i` in the global scope
    auto* fnI = sym.declareFuncName("i");
    REQUIRE(
        sym.setFuncSig(fnI, sema::FunctionSignature({ sym.S64() }, sym.S64())));
    auto* xType = sym.declareStructureType("X");
    REQUIRE(xType);
    auto* memberI = sym.withScopePushed(xType, [&] {
        return sym.defineVariable("i", sym.S64(), Mutable);
    });
    xType->setSize(8);
    auto const* const overloadSet = sym.lookup<sema::OverloadSet>("i");
    REQUIRE(overloadSet != nullptr);
    sym.pushScope(xType);
    auto const* const memberVar = sym.lookup<sema::Variable>("i");
    REQUIRE(memberVar != nullptr);
    CHECK(memberI == memberVar);
    sym.popScope();
}
