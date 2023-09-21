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
                         sema::FunctionSignature({ sym.S64() }, sym.S64()));
    CHECK(f_int_success);
    /// Declare a function ` f: (double) -> double `
    auto* f_double = &sym.declareFunction("f").value();
    auto const f_double_success =
        sym.setSignature(f_double,
                         sema::FunctionSignature({ sym.F64() }, sym.F64()));
    CHECK(f_double_success);
    /// Declare a function ` f: (double) -> int `
    auto* f_double2 = &sym.declareFunction("f").value();
    auto const f_double2_success =
        sym.setSignature(f_double2,
                         sema::FunctionSignature({ sym.F64() }, sym.S64()));
    REQUIRE(!f_double2_success);
    auto* f2error =
        dynamic_cast<sema::InvalidDeclaration*>(f_double2_success.error());
    CHECK(f2error->reason() == sema::InvalidDeclaration::Reason::Redefinition);
    /// Declare a function `fn f(double) -> double`
    auto* f_double3 = &sym.declareFunction("f").value();
    auto const f_double3_success =
        sym.setSignature(f_double3,
                         sema::FunctionSignature({ sym.F64() }, sym.F64()));
    CHECK(!f_double3_success);
    auto* f3error =
        dynamic_cast<sema::InvalidDeclaration*>(f_double3_success.error());
    CHECK(f3error->reason() == sema::InvalidDeclaration::Reason::Redefinition);
}
