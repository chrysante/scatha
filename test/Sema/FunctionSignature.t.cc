#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Function Type", "[sema]") {
    SymbolTable sym;
    auto const fSig = FunctionSignature({ sym.S64() }, sym.S64());
    auto const gSig = FunctionSignature({ sym.S64() }, sym.Void());
    auto* fnF = sym.declareFuncName("f");
    REQUIRE(sym.setFuncSig(fnF, fSig));
    CHECK(fnF->signature().argumentType(0) == sym.S64());
    CHECK(fnF->signature().returnType() == sym.S64());
    auto const fnG = sym.declareFuncName("g");
    REQUIRE(sym.setFuncSig(fnG, gSig));
    CHECK(fnG->signature().argumentType(0) == sym.S64());
    CHECK(fnG->signature().returnType() == sym.Void());
}
