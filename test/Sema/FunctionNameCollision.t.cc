#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using namespace ast;

TEST_CASE("Define two functions with the same signature", "[sema]") {
    std::string const text = R"(
fn f(x: int) -> int {
	return 0;
}
fn g(x: int) -> int {
	return 1;
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    auto* f = sym.unqualifiedLookup("f").front();
    CHECK(isa<Function>(f));
    CHECK(f->name() == "f");
    auto* g = sym.unqualifiedLookup("g").front();
    CHECK(isa<Function>(g));
    CHECK(g->name() == "g");
}
