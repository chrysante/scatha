#include <Catch/Catch2.hpp>

#include "Parser/ParsingIssue.h"
#include "Sema/SemanticIssue.h"
#include "test/Sema/SimpleAnalzyer.h"

TEST_CASE("Use of undeclared identifier") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() -> int { return x; }"), UseOfUndeclaredIdentifier);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let v: UnknownType; }"), UseOfUndeclaredIdentifier);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { 1 + x; }"), UseOfUndeclaredIdentifier);
}

TEST_CASE("Invalid type conversion") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() -> int { return \"a string\"; }"), BadTypeConversion);
}

TEST_CASE("Invalid function call expression") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
 fn callee(a: string) {}
 fn caller() { callee(); }
)"), BadFunctionCall);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
 fn callee(a: string) {}
 fn caller() { callee(0); }
)"), BadTypeConversion);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x: float = 1; }"), BadTypeConversion);
	
	CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("fn f() { let x: float = 1.; }"));
}

TEST_CASE("Invalid function redeclaration") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
fn f() {}
fn f() -> int {}
)"), InvalidFunctionDeclaration);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
fn f() {}
fn f() {}
)"), InvalidFunctionDeclaration);
}

TEST_CASE("Invalid variable redeclaration") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(R"(
fn f(x: int) {
	let x: float;
}
)"), InvalidRedeclaration);
	
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(x: int, x: int) {}"), InvalidRedeclaration);
}

TEST_CASE("Invalid redeclaration category") {
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
	
TEST_CASE("Invalid symbol reference") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(param: UnknownID) {}"), UseOfUndeclaredIdentifier);
}
		
TEST_CASE("Invalid variable declaration") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let v; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x = 0; let y: x; }"), InvalidStatement);
}

TEST_CASE("Invalid function declaration") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { fn g(); }"), parse::ParsingIssue);
}

TEST_CASE("Invalid struct declaration") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { struct X; }"), parse::ParsingIssue);
}

TEST_CASE("Invalid statement at struct scope") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { return 0; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { 1; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { 1 + 2; }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { if (1 > 0) {} }"), InvalidStatement);
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { while (1 > 0) {} }"), InvalidStatement);
	CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("struct X { var i: int; }"));
}

TEST_CASE("Invalid local scope in struct") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { {} }"), InvalidStatement);
}

TEST_CASE("Valid local scope in function") {
	CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("fn f() { {} }"));
}

TEST_CASE("Other semantic errors") {
	CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x = int; }"), SemanticIssue);
}
