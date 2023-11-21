#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Function Type", "[sema]") {
    SymbolTable sym;
    auto const fSig = FunctionSignature({ sym.S64() }, sym.S64());
    auto const gSig = FunctionSignature({ sym.S64() }, sym.Void());
    auto* F = sym.declareFunction("f", fSig);
    REQUIRE(F);
    CHECK(F->signature().argumentType(0) == sym.S64());
    CHECK(F->signature().returnType() == sym.S64());
    auto const G = sym.declareFunction("g", gSig);
    REQUIRE(G);
    CHECK(G->signature().argumentType(0) == sym.S64());
    CHECK(G->signature().returnType() == sym.Void());
}
