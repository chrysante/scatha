#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("SymbolTable lookup", "[sema]") {
    sema::SymbolTable sym;
    auto const var = sym.addVariable("x", sym.qualInt());
    REQUIRE(var.hasValue());
    auto* x = sym.lookup("x");
    CHECK(&var.value() == x);
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    // Define function 'i' in the global scope
    auto* fnI = &sym.declareFunction("i").value();
    auto const overloadSuccessI =
        sym.setSignature(fnI,
                         sema::FunctionSignature({ sym.qualInt() },
                                                 sym.qualInt()));
    REQUIRE(overloadSuccessI);
    auto* xType = &sym.declareStructureType("X").value();
    // Begin X
    sym.pushScope(xType);
    // Add member variable 'i' to
    auto* memberI = &sym.addVariable("i", sym.qualInt()).value();
    sym.popScope();
    // End X
    xType->setSize(8);
    auto const* const overloadSet = sym.lookup<sema::OverloadSet>("i");
    REQUIRE(overloadSet != nullptr);
    auto const* fnILookup = overloadSet->find(std::array{ sym.qualInt() });
    CHECK(fnI == fnILookup);
    sym.pushScope(xType);
    auto const* const memberVar = sym.lookup<sema::Variable>("i");
    REQUIRE(memberVar != nullptr);
    CHECK(memberI == memberVar);
    sym.popScope();
}
