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
    auto const xID = sym.lookup("x");
    CHECK(var.value().symbolID() == xID);
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    // Define function 'i' in the global scope
    auto const fnI = sym.declareFunction("i");
    REQUIRE(fnI.hasValue());
    auto const overloadSuccessI =
        sym.setSignature(fnI->symbolID(),
                         sema::FunctionSignature({ sym.qualInt() },
                                                 sym.qualInt()));
    REQUIRE(overloadSuccessI);
    auto& xType = sym.declareObjectType("X").value();
    // Begin X
    sym.pushScope(xType.symbolID());
    // Add member variable 'i' to
    auto const memberI = sym.addVariable("i", sym.qualInt());
    sym.popScope();
    // End X
    xType.setSize(8);
    auto const* const overloadSet = sym.lookup<sema::OverloadSet>("i");
    REQUIRE(overloadSet != nullptr);
    auto const* fnILookup = overloadSet->find(std::array{ sym.qualInt() });
    CHECK(&fnI.value() == fnILookup);
    sym.pushScope(xType.symbolID());
    auto const* const memberVar = sym.lookup<sema::Variable>("i");
    REQUIRE(memberVar != nullptr);
    CHECK(&memberI.value() == memberVar);
    sym.popScope();
}
