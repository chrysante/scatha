#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("OverloadSet") {
    sema::SymbolTable sym;
    /// Declare a function \code f: (int) -> int
    auto* f_int = &sym.declareFunction("f").value();
    auto const f_int_success =
        sym.setSignature(f_int,
                         sema::FunctionSignature({ sym.qS64() }, sym.qS64()));
    CHECK(f_int_success);
    /// Declare a function \code f: (float) -> float
    auto* f_float = &sym.declareFunction("f").value();
    auto const f_float_success =
        sym.setSignature(f_float,
                         sema::FunctionSignature({ sym.qFloat() },
                                                 sym.qFloat()));
    CHECK(f_float_success);
    /// Declare a function \code f: (float) -> int
    auto* f_float2 = &sym.declareFunction("f").value();
    auto const f_float2_success =
        sym.setSignature(f_float2,
                         sema::FunctionSignature({ sym.qFloat() }, sym.qS64()));
    REQUIRE(!f_float2_success);
    auto* f2error =
        dynamic_cast<sema::InvalidDeclaration*>(f_float2_success.error());
    CHECK(f2error->reason() ==
          sema::InvalidDeclaration::Reason::CantOverloadOnReturnType);
    /// Declare a function \code f: (float) -> float
    auto* f_float3 = &sym.declareFunction("f").value();
    auto const f_float3_success =
        sym.setSignature(f_float3,
                         sema::FunctionSignature({ sym.qFloat() },
                                                 sym.qFloat()));
    CHECK(!f_float3_success);
    auto* f3error =
        dynamic_cast<sema::InvalidDeclaration*>(f_float3_success.error());
    CHECK(f3error->reason() == sema::InvalidDeclaration::Reason::Redefinition);
}
