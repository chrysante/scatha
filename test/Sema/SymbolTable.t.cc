#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/OverloadSet.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("SymbolTable lookup", "[sema]") {
    sema::SymbolTable sym;
    auto const var = sym.addVariable(Token("x", TokenType::Identifier), sym.Int());
    REQUIRE(var.hasValue());
    auto const xID = sym.lookup("x");
    CHECK(var.value().symbolID() == xID);
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
    sema::SymbolTable sym;
    // Define function 'i' in the global scope
    auto const fnI = sym.declareFunction(Token("i", TokenType::Identifier));
    REQUIRE(fnI.hasValue());
    auto const overloadSuccessI = sym.setSignature(fnI->symbolID(), sema::FunctionSignature({ sym.Int() }, sym.Int()));
    REQUIRE(overloadSuccessI);
    auto& xType = sym.declareObjectType(Token("X", TokenType::Identifier)).value();
    // Begin X
    sym.pushScope(xType.symbolID());
    // Add member variable 'i' to
    auto const memberI = sym.addVariable(Token("i", TokenType::Identifier), sym.Int());
    sym.popScope();
    // End X
    xType.setSize(8);
    auto const* const overloadSet = sym.lookupOverloadSet(Token("i", TokenType::Identifier));
    REQUIRE(overloadSet != nullptr);
    auto const* fnILookup = overloadSet->find(std::array{ sym.Int() });
    CHECK(&fnI.value() == fnILookup);
    sym.pushScope(xType.symbolID());
    auto const* const memberVar = sym.lookupVariable(Token("i", TokenType::Identifier));
    REQUIRE(memberVar != nullptr);
    CHECK(&memberI.value() == memberVar);
    sym.popScope();
}
