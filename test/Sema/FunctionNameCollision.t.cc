#include <Catch/Catch2.hpp>

#include "AST/AST.h"
#include "Parser/ParsingIssue.h"
#include "Sema/SemanticIssue.h"
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
}
)";

    auto const [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);

    auto f = sym.lookup("f");
    CHECK(f.category() == SymbolCategory::OverloadSet);
    CHECK(sym.getOverloadSet(f).name() == "f");

    auto g = sym.lookup("g");
    CHECK(g.category() == SymbolCategory::OverloadSet);
    CHECK(sym.getOverloadSet(g).name() == "g");
}
