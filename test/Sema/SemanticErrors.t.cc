#include <Catch/Catch2.hpp>

#include "Parser/ParsingIssue.h"
#include "Sema/SemanticIssue.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Use of undeclared identifier", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() -> int { return x; }"), UseOfUndeclaredIdentifier);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let v: UnknownType; }"), UseOfUndeclaredIdentifier);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { 1 + x; }"), UseOfUndeclaredIdentifier);
}

TEST_CASE("Invalid type conversion", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() -> int { return \"a string\"; }"), BadTypeConversion);
}

TEST_CASE("Invalid function call expression", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
 fn callee(a: string) {}
 fn caller() { callee(); }
)"), BadFunctionCall);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
 fn callee(a: string) {}
 fn caller() { callee(0); }
)"), BadFunctionCall);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x: float = 1; }"), BadTypeConversion);
	
	CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("fn f() { let x: float = 1.; }"));
}

TEST_CASE("Invalid function redeclaration", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
fn f() {}
fn f() -> int {}
)"), InvalidRedeclaration);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
fn f() {}
fn f() {}
)"), InvalidRedeclaration);
}

TEST_CASE("Invalid variable redeclaration", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
fn f(x: int) {
	let x: float;
}
)"), InvalidRedeclaration);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(x: int, x: int) {}"), InvalidRedeclaration);
}

TEST_CASE("Invalid redeclaration category", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct f{}"
														 "fn f(){}"),
					InvalidRedeclaration);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(){}"
														 "struct f{}"),
					InvalidRedeclaration);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct f;"
														 "struct f;"
														 "struct f {}"),
					parse::ParsingIssue);
}
	
TEST_CASE("Invalid symbol reference", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(param: UnknownID) {}"), UseOfUndeclaredIdentifier);
}
		
TEST_CASE("Invalid variable declaration", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let v; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x = 0; let y: x; }"), InvalidSymbolReference);
}

TEST_CASE("Invalid function declaration", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { fn g() {} }"), InvalidFunctionDeclaration);
}

TEST_CASE("Invalid struct declaration", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { struct X; }"), parse::ParsingIssue);
}

TEST_CASE("Invalid statement at struct scope", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { return 0; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { 1; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { 1 + 2; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { if (1 > 0) {} }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { while (1 > 0) {} }"), InvalidStatement);
	CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("struct X { var i: int; }"));
}

TEST_CASE("Invalid local scope in struct", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { {} }"), InvalidStatement);
}

TEST_CASE("Valid local scope in function", "[sema]") {
	CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("fn f() { {} }"));
}

TEST_CASE("Other semantic errors", "[sema]") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x = int; }"), SemanticIssue);
}

TEST_CASE("Mutual reference in struct definition", "[sema]") {
	auto const text = R"(
struct X { var y: Y; }
struct Y { var x: X; }
)";
	// TODO: Check specific exception
	CHECK_THROWS(test::produceDecoratedASTAndSymTable(text));
}
