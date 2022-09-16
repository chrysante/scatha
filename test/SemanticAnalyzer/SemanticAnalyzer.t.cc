#include <Catch/Catch2.hpp>

#include <iostream>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "SemanticAnalyzer/SemanticAnalyzer.h"
#include "SemanticAnalyzer/SemanticError.h"

using namespace scatha;
using namespace lex;
using namespace parse;
using namespace sem;
using namespace ast;

static auto produceDecoratedASTAndSymTable(std::string text) {
	Lexer l(text);
	auto tokens = l.lex();
	Parser p(tokens);
	auto ast = p.parse();

	SemanticAnalyzer s;
	s.run(ast.get());
	
	return std::tuple(std::move(ast), s.takeSymbolTable());
}

TEST_CASE("Registration in SymbolTable", "[sem]") {
	std::string const text = R"(

fn mul(a: int, b: int, c: float) -> int {
	let result = a;
	return result;
}

)";

	auto [ast, sym] = produceDecoratedASTAndSymTable(text);
	
	auto const& symMul = sym.lookupName(Token("mul"));
	CHECK(symMul.category() == NameCategory::Function);
	
	auto const& fnMul = sym.getFunction(symMul);
	auto const& fnType = sym.getType(fnMul.typeID());
	
	CHECK(fnType.returnType() == sym.Int());
	REQUIRE(fnType.argumentTypes().size() == 3);
	CHECK(fnType.argumentType(0) == sym.Int());
	CHECK(fnType.argumentType(1) == sym.Int());
	CHECK(fnType.argumentType(2) == sym.Float());
	
	auto const  mulScopeID = sym.globalScope()->findIDByName("mul");
	auto const* mulScope   = sym.globalScope()->childScope(mulScopeID.value());
	
	auto const aID = mulScope->findIDByName("a").value();
	auto const& a = sym.getVariable(aID);
	CHECK(a.typeID() == sym.Int());
	
	auto const bID = mulScope->findIDByName("b").value();
	auto const& b = sym.getVariable(bID);
	CHECK(b.typeID() == sym.Int());
	
	auto const cID = mulScope->findIDByName("c").value();
	auto const& c = sym.getVariable(cID);
	CHECK(c.typeID() == sym.Float());
	
	auto const resultID = mulScope->findIDByName("result").value();
	auto const& result = sym.getVariable(resultID);
	CHECK(result.typeID() == sym.Int());
}

TEST_CASE("Decoration of the AST with function call expression", "[sem]") {
	std::string const text = R"(

fn callee(a: string, b: int) -> float;

fn caller() -> float {
	let result = callee("Hello world", 0);
	return result;
}

)";

	auto [ast, sym] = produceDecoratedASTAndSymTable(text);
	
	auto* tu = dynamic_cast<TranslationUnit*>(ast.get());
	REQUIRE(tu);
	auto* calleeDecl = dynamic_cast<FunctionDeclaration*>(tu->declarations[0].get());
	REQUIRE(calleeDecl);
	CHECK(calleeDecl->returnTypeID == sym.Float());
	CHECK(calleeDecl->parameters[0]->typeID == sym.String());
	CHECK(calleeDecl->parameters[1]->typeID == sym.Int());
	
	auto* caller = dynamic_cast<FunctionDefinition*>(tu->declarations[1].get());
	REQUIRE(caller);
	
	auto* resultDecl = dynamic_cast<VariableDeclaration*>(caller->body->statements[0].get());
	CHECK(resultDecl->initExpression->typeID == sym.Float());
}

TEST_CASE("Semantic analysis failures", "[sem]") {
	SECTION("Use of undeclared identifier") {
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() -> int { return x; }"), UseOfUndeclaredIdentifier);
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() { let v: UnknownType; }"), UseOfUndeclaredIdentifier);
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() { 1 + x; }"), UseOfUndeclaredIdentifier);
	}
	
	SECTION("Invalid type conversion") {
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() -> int { return \"a string\"; }"), BadTypeConversion);
	}
	
	SECTION("Invalid function call expression") {
		std::string const a = R"(
fn callee(a: string);
fn caller() { callee(); }
)";
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable(a), BadFunctionCall);
	
		std::string const b = R"(
fn callee(a: string);
fn caller() { callee(0); }
)";
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable(b), BadTypeConversion);
	}
	
	SECTION("Invalid function redeclaration") {
		std::string const a = R"(
fn f();
fn f() -> int;
)";
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable(a), InvalidRedeclaration);
// MARK: These tests will fail when we have function overloading
		std::string const b = R"(
fn f();
fn f(x: int);
)";
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable(b), InvalidRedeclaration);
		std::string const c = R"(
fn f(x: string);
fn f(x: int);
)";
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable(b), InvalidRedeclaration);
	}
	
	SECTION("Invalid symbol reference") {
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() -> UnknownID;"), InvalidSymbolReference);
	}
	
	SECTION("Invalid variable declaration") {
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() { let v; }"), InvalidStatement);
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() { let x = 0; let y: x; }"), InvalidStatement);
	}
	
	SECTION("Other semantic errors") {
		CHECK_THROWS_AS(produceDecoratedASTAndSymTable("fn f() { let x = int; }"), SemanticError);
	}
	
}

