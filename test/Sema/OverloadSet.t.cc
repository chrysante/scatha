#include <catch2/catch_test_macros.hpp>

#include "Issue/IssueHandler.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("OverloadSet", "[sema]") {
    IssueHandler iss;
    SymbolTable sym;
    sym.setIssueHandler(iss);
    /// Declare a function \code f: (int) -> int
    auto* f_int =
        sym.declareFunction("f",
                            sym.functionType({ sym.S64() }, sym.S64()),
                            AccessControl::Public);
    REQUIRE(f_int);
    /// Declare a function ` f: (double) -> double `
    auto* f_double =
        sym.declareFunction("f",
                            sym.functionType({ sym.F64() }, sym.F64()),
                            AccessControl::Public);
    REQUIRE(f_double);
    /// Declare a function ` f: (double) -> int `
    auto* f_double2 =
        sym.declareFunction("f",
                            sym.functionType({ sym.F64() }, sym.S64()),
                            AccessControl::Public);
    CHECK(!f_double2);
    CHECK((!iss.empty() &&
           dynamic_cast<sema::Redefinition const*>(&iss.front())));
    iss.clear();
    /// Declare a function `fn f(double) -> double`
    auto* f_double3 =
        sym.declareFunction("f",
                            sym.functionType({ sym.F64() }, sym.F64()),
                            AccessControl::Public);
    CHECK(!f_double3);
    CHECK((!iss.empty() &&
           dynamic_cast<sema::Redefinition const*>(&iss.front())));
}
