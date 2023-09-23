#include <Catch/Catch2.hpp>

#include "Issue/IssueHandler.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("OverloadSet") {
    IssueHandler iss;
    sema::SymbolTable sym;
    sym.setIssueHandler(iss);
    /// Declare a function \code f: (int) -> int
    auto* f_int = sym.declareFuncName("f");
    CHECK(sym.setFuncSig(f_int,
                         sema::FunctionSignature({ sym.S64() }, sym.S64())));
    /// Declare a function ` f: (double) -> double `
    auto* f_double = sym.declareFuncName("f");
    CHECK(sym.setFuncSig(f_double,
                         sema::FunctionSignature({ sym.F64() }, sym.F64())));
    /// Declare a function ` f: (double) -> int `
    auto* f_double2 = sym.declareFuncName("f");
    REQUIRE(!sym.setFuncSig(f_double2,
                            sema::FunctionSignature({ sym.F64() }, sym.S64())));
    CHECK(dynamic_cast<sema::Redefinition const*>(&iss.front()));
    iss.clear();
    /// Declare a function `fn f(double) -> double`
    auto* f_double3 = sym.declareFuncName("f");
    REQUIRE(!sym.setFuncSig(f_double3,
                            sema::FunctionSignature({ sym.F64() }, sym.F64())));
    CHECK(dynamic_cast<sema::Redefinition const*>(&iss.front()));
}
