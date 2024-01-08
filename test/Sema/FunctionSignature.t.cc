#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Function Type", "[sema]") {
    SymbolTable sym;
    auto* fSig = sym.functionType({ sym.S64() }, sym.S64());
    auto* gSig = sym.functionType({ sym.S64() }, sym.Void());
    auto* F = sym.declareFunction("f", fSig, AccessControl::Public);
    REQUIRE(F);
    CHECK(F->argumentType(0) == sym.S64());
    CHECK(F->returnType() == sym.S64());
    auto const G = sym.declareFunction("g", gSig, AccessControl::Public);
    REQUIRE(G);
    CHECK(G->argumentType(0) == sym.S64());
    CHECK(G->returnType() == sym.Void());
}
