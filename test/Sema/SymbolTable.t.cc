#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/Exp/OverloadSet.h"
#include "Sema/Exp/SymbolTable.h"

using namespace scatha;

TEST_CASE("SymbolTable lookup", "[sema]") {
	sema::exp::SymbolTable sym;
	
	auto const var = sym.addVariable("x", sym.Int());
	REQUIRE(var.hasValue());
	auto const xID = sym.lookup("x");
	CHECK(var.value().symbolID() == xID);
}

TEST_CASE("SymbolTable define custom type", "[sema]") {
	sema::exp::SymbolTable sym;
	
	// define function f in the global scope
	auto const fnI = sym.addFunction("i", sema::exp::FunctionSignature({ sym.Int() }, sym.Int()));
	CHECK(fnI.hasValue());
	
	auto& type = sym.addObjectType("X").value();
	
	// --- Add to X ---
	sym.pushScope(type.symbolID());
	
	// Add member variable 'i' to
	auto const memberI = sym.addVariable("i", sym.Int());
	
	
//	sym.addFunction("f", <#FunctionSignature#>)
	
	sym.popScope();
	// --- ---
	
	type.setSize(8);
	
	auto const* const overloadSet = sym.lookupOverloadSet("i");
	REQUIRE(overloadSet != nullptr);
	auto const* fnILookup = overloadSet->find(std::array{ sym.Int() });
	CHECK(&fnI.value() == fnILookup);
	
	sym.pushScope(type.symbolID());
	auto const* const memberVar = sym.lookupVariable("i");
	REQUIRE(memberVar != nullptr);
	CHECK(&memberI.value() == memberVar);
	sym.popScope();
}
