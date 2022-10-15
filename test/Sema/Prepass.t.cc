#include <Catch/Catch2.hpp>

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Prepass.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;

static std::tuple<sema::SymbolTable, issue::IssueHandler> doPrepass(std::string_view text) {
    lex::Lexer l(text);
    auto tokens = l.lex();
    parse::Parser p(tokens);
    auto ast = p.parse();
    issue::IssueHandler iss;
    auto sym = sema::prepass(*ast, iss);
    return { std::move(sym), std::move(iss) };
}

TEST_CASE("Prepass", "[sema]") {
    auto const text       = R"(
fn f(x: X) -> int { return 0; }

struct X {
	var i: int;
	var y: Y;
}

struct Y {
	var i: int;
}
)";

    auto const [sym, iss] = doPrepass(text);
    auto const* X         = sym.lookupObjectType("X");
    REQUIRE(X);
    CHECK(X->size() == 16);
    auto const* Y = sym.lookupObjectType("Y");
    REQUIRE(Y);
    CHECK(Y->size() == 8);
    auto const* fOS = sym.lookupOverloadSet("f");
    REQUIRE(fOS);
    auto const* f = fOS->find(std::array{ X->symbolID() });
    REQUIRE(f);
}

TEST_CASE("Prepass 2", "[sema]") {
    auto const text = R"(
struct X {
	fn f(y: Y) {}
	struct Y {}
}
)";

    auto [sym, iss] = doPrepass(text);
    auto const* X   = sym.lookupObjectType("X");
    REQUIRE(X);
    sym.pushScope(X->symbolID());
    auto const* Y = sym.lookupObjectType("Y");
    REQUIRE(Y);
    auto const* fOS = sym.lookupOverloadSet("f");
    REQUIRE(fOS);
    auto const* f = fOS->find(std::array{ Y->symbolID() });
    REQUIRE(f);
    sym.popScope();
}

TEST_CASE("Struct size and align", "[sema]") {
    auto const text            = R"(
	struct X {
		var y: Y;
		var a: bool;
		var b: bool;
		var c: bool;
		var x: int;
		var d: bool;
	}
	struct Y { var i: int; }
)";

    auto const [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    auto const* X              = sym.lookupObjectType("X");
    REQUIRE(X);
    CHECK(X->size() == 32);
    CHECK(X->align() == 8);
    auto const* Y = sym.lookupObjectType("Y");
    CHECK(Y->size() == 8);
    CHECK(Y->align() == 8);
}
