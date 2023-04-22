#include <Catch/Catch2.hpp>

#include "Sema/OverloadSet.h"
#include "Sema/SemaIssue2.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("OverloadSet") {
    sema::SymbolTable sym;
    /// Declare a function \code f: (int) -> int
    auto f_int = sym.declareFunction("f");
    REQUIRE(f_int.hasValue());
    auto const f_int_success =
        sym.setSignature(f_int->symbolID(),
                         sema::FunctionSignature({ sym.Int() }, sym.Int()));
    CHECK(f_int_success);
    /// Declare a function \code f: (float) -> float
    auto f_float = sym.declareFunction("f");
    REQUIRE(f_float.hasValue());
    auto const f_float_success =
        sym.setSignature(f_float->symbolID(),
                         sema::FunctionSignature({ sym.Float() }, sym.Float()));
    CHECK(f_float_success);
    /// Declare a function \code f: (float) -> int
    auto f_float2 = sym.declareFunction("f");
    REQUIRE(f_float2.hasValue());
    auto const f_float2_success =
        sym.setSignature(f_float2->symbolID(),
                         sema::FunctionSignature({ sym.Float() }, sym.Int()));
    REQUIRE(!f_float2_success);
    auto* f2error =
        dynamic_cast<sema::InvalidDeclaration*>(f_float2_success.error());
    CHECK(f2error->reason() ==
          sema::InvalidDeclaration::Reason::CantOverloadOnReturnType);
    /// Declare a function \code f: (float) -> float
    auto f_float3 = sym.declareFunction("f");
    REQUIRE(f_float3.hasValue());
    auto const f_float3_success =
        sym.setSignature(f_float3->symbolID(),
                         sema::FunctionSignature({ sym.Float() }, sym.Float()));
    CHECK(!f_float3_success);
    auto* f3error =
        dynamic_cast<sema::InvalidDeclaration*>(f_float3_success.error());
    CHECK(f3error->reason() == sema::InvalidDeclaration::Reason::Redefinition);
}
