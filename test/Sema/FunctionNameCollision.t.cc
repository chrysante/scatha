#include <Catch/Catch2.hpp>

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
    auto const [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    auto* f = sym.lookup("f");
    CHECK(isa<OverloadSet>(f));
    CHECK(f->name() == "f");
    auto* g = sym.lookup("g");
    CHECK(isa<OverloadSet>(g));
    CHECK(g->name() == "g");
}
