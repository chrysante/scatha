#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/FunctionSignature.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Function Type", "[sema]") {
    SymbolTable sym;
    auto const fSig = FunctionSignature({ sym.qS64() }, sym.qS64());
    auto const gSig = FunctionSignature({ sym.qS64() }, sym.qVoid());
    CHECK(argumentsEqual(fSig, gSig));
    auto* fnF                  = &sym.declareFunction("f").value();
    auto const overloadSuccess = sym.setSignature(fnF, fSig);
    REQUIRE(overloadSuccess);
    CHECK(fnF->signature().argumentType(0)->base() == sym.S64());
    CHECK(fnF->signature().returnType()->base() == sym.S64());
    auto const fnG              = &sym.declareFunction("g").value();
    auto const overloadSuccess2 = sym.setSignature(fnG, gSig);
    REQUIRE(overloadSuccess2);
    CHECK(fnG->signature().argumentType(0)->base() == sym.S64());
    CHECK(fnG->signature().returnType()->base() == sym.Void());
}
