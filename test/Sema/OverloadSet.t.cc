#include <Catch/Catch2.hpp>

#include "Sema/Exp/OverloadSet.h"
#include "Sema/Exp/SymbolTable.h"

using namespace scatha;

TEST_CASE("OverloadSet") {
	sema::exp::SymbolTable sym;
	// Declare a function f: (int) -> int
	auto f_int = sym.addFunction("f", sema::exp::FunctionSignature({ sym.Int() }, sym.Int()));
	CHECK(f_int.hasValue());
	
	// Declare a function f: (float) -> float
	auto f_float = sym.addFunction("f", sema::exp::FunctionSignature({ sym.Float() }, sym.Float()));
	CHECK(f_float.hasValue());

	// Declare a function f: (float) -> int
	auto f_float2 = sym.addFunction("f", sema::exp::FunctionSignature({ sym.Float() }, sym.Int()));
	REQUIRE(!f_float2.hasValue());
	CHECK(f_float2.error().reason() == sema::exp::OverloadIssue::CantOverloadOnReturnType);
	
	// Declare a function f: (float) -> float
	auto f_float3 = sym.addFunction("f", sema::exp::FunctionSignature({ sym.Float() }, sym.Float()));
	REQUIRE(!f_float3.hasValue());
	CHECK(f_float3.error().reason() == sema::exp::OverloadIssue::Redefinition);
	
}
