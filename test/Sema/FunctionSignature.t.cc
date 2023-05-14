#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Function Type", "[sema]") {
    SymbolTable sym;
    auto const fSig            = FunctionSignature({ sym.S64() }, sym.S64());
    auto const gSig            = FunctionSignature({ sym.S64() }, sym.Void());
    auto* fnF                  = &sym.declareFunction("f").value();
    auto const overloadSuccess = sym.setSignature(fnF, fSig);
    REQUIRE(overloadSuccess);
    CHECK(fnF->signature().argumentType(0)->base() == sym.rawS64());
    CHECK(fnF->signature().returnType()->base() == sym.rawS64());
    auto const fnG              = &sym.declareFunction("g").value();
    auto const overloadSuccess2 = sym.setSignature(fnG, gSig);
    REQUIRE(overloadSuccess2);
    CHECK(fnG->signature().argumentType(0)->base() == sym.rawS64());
    CHECK(fnG->signature().returnType()->base() == sym.rawVoid());
}
