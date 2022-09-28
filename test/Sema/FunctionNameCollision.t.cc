#include <Catch/Catch2.hpp>

#include "AST/AST.h"
#include "AST/Expression.h"
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
	
	auto const [ast, sym] = test::produceDecoratedASTAndSymTable(text);
	
	auto f = sym.lookupName(Token("f"));
	CHECK(f.category() == SymbolCategory::Function);
	CHECK(sym.getFunction(f).name() == "f");
	
	auto g = sym.lookupName(Token("g"));
	CHECK(g.category() == SymbolCategory::Function);
	CHECK(sym.getFunction(g).name() == "g");
}
