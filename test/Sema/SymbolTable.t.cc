#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("SymbolTable lookup", "[sema]") {
    sema::SymbolTable sym;
    auto const var = sym.addVariable("x", sym.S64());
    REQUIRE(var.hasValue());
    auto* x = sym.lookup("x");
    CHECK(&var.value() == x);
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    /// Declare function `i` in the global scope
    auto* fnI = &sym.declareFunction("i").value();
    auto const overloadSuccessI =
        sym.setSignature(fnI,
                         sema::FunctionSignature({ sym.S64() }, sym.S64()));
    REQUIRE(overloadSuccessI);
    auto* xType = &sym.declareStructureType("X").value();
    /// Begin `struct X`
    sym.pushScope(xType);
    /// Add member variable `i` to `X`
    auto* memberI = &sym.addVariable("i", sym.S64()).value();
    sym.popScope();
    /// End `X`
    xType->setSize(8);
    auto const* const overloadSet = sym.lookup<sema::OverloadSet>("i");
    REQUIRE(overloadSet != nullptr);
    sym.pushScope(xType);
    auto const* const memberVar = sym.lookup<sema::Variable>("i");
    REQUIRE(memberVar != nullptr);
    CHECK(memberI == memberVar);
    sym.popScope();
}
