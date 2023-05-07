#include <Catch/Catch2.hpp>

#include <array>

#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

static void makeFunction();

TEST_CASE("...", "[sema]") {
    sema::SymbolTable sym;
    auto const var = sym.addVariable("x", sym.qS64());
    REQUIRE(var.hasValue());
    auto* x = sym.lookup("x");
    CHECK(&var.value() == x);
}
