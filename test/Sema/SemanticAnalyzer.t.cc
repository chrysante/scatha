#include <Catch/Catch2.hpp>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Parser/ParsingIssue.h"
#include "Sema/SemanticIssue.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using namespace ast;

TEST_CASE("Registration in SymbolTable", "[sema]") {
	std::string const text = R"(

fn mul(a: int, b: int, c: float) -> int {
	let result = a;
	return result;
}

)";

	auto [ast, sym] = test::produceDecoratedASTAndSymTable(text);
	
	auto const& symMul = sym.lookupName(Token("mul"));
	CHECK(symMul.category() == SymbolCategory::Function);
	
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

TEST_CASE("Decoration of the AST") {
	std::string const text = R"(
fn mul(a: int, b: int, c: float, d: string) -> int {
	let result = a;
	{ // declaration of variable of the same name in a nested scope
		var result = "some string";
	}
	// declaration of float variable
	let y = 39;
	let z = 0x39E;
	let x = 1.2;
	return result;
}
)";
	auto [ast, sym] = test::produceDecoratedASTAndSymTable(text);
	
	auto* tu = downCast<TranslationUnit>(ast.get());
	auto* fnDecl = downCast<FunctionDefinition>(tu->declarations[0].get());
	CHECK(fnDecl->returnTypeID == sym.Int());
	CHECK(fnDecl->parameters[0]->typeID == sym.Int());
	CHECK(fnDecl->parameters[1]->typeID == sym.Int());
	CHECK(fnDecl->parameters[2]->typeID == sym.Float());
	CHECK(fnDecl->parameters[3]->typeID == sym.String());
	
	auto* fn = downCast<FunctionDefinition>(tu->declarations[0].get());
	CHECK(fn->returnTypeID == sym.Int());
	CHECK(fn->parameters[0]->typeID == sym.Int());
	CHECK(fn->parameters[1]->typeID == sym.Int());
	CHECK(fn->parameters[2]->typeID == sym.Float());
	CHECK(fn->parameters[3]->typeID == sym.String());

	auto* varDecl = downCast<VariableDeclaration>(fn->body->statements[0].get());
	CHECK(varDecl->typeID == sym.Int());
	
	auto* varDeclInit = downCast<Identifier>(varDecl->initExpression.get());
	CHECK(varDeclInit->typeID == sym.Int());

	auto* nestedScope = downCast<Block>(fn->body->statements[1].get());
	
	auto* nestedVarDecl = downCast<VariableDeclaration>(nestedScope->statements[0].get());
	CHECK(nestedVarDecl->typeID == sym.String());
	
	auto* xDecl = downCast<VariableDeclaration>(fn->body->statements[2].get());
	CHECK(xDecl->typeID == sym.Int());
	auto* intLit = downCast<IntegerLiteral>(xDecl->initExpression.get());
	CHECK(intLit->value == 39);
	
	auto* zDecl = downCast<VariableDeclaration>(fn->body->statements[3].get());
	CHECK(zDecl->typeID == sym.Int());
	auto* intHexLit = downCast<IntegerLiteral>(zDecl->initExpression.get());
	CHECK(intHexLit->value == 0x39E);
	
	auto* yDecl = downCast<VariableDeclaration>(fn->body->statements[4].get());
	CHECK(yDecl->typeID == sym.Float());
	auto* floatLit = downCast<FloatingPointLiteral>(yDecl->initExpression.get());
	CHECK(floatLit->value == 1.2);
	
	auto* ret = downCast<ReturnStatement>(fn->body->statements[5].get());
	auto* retIdentifier = downCast<Identifier>(ret->expression.get());
	CHECK(retIdentifier->typeID == sym.Int());
}

TEST_CASE("Decoration of the AST with function call expression", "[sema]") {
	std::string const text = R"(

fn callee(a: string, b: int, c: bool) -> float { return 0.0; }

fn caller() -> float {
	let result = callee("Hello world", 0, true);
	return result;
}

)";

	auto [ast, sym] = test::produceDecoratedASTAndSymTable(text);
	
	auto* tu = downCast<TranslationUnit>(ast.get());
	REQUIRE(tu);
	auto* calleeDecl = downCast<FunctionDefinition>(tu->declarations[0].get());
	REQUIRE(calleeDecl);
	CHECK(calleeDecl->returnTypeID == sym.Float());
	auto calleeArgTypes = { sym.String(), sym.Int(), sym.Bool() };
	auto const& functionType = sym.getType(computeFunctionTypeID(sym.Float(), calleeArgTypes));
	CHECK(calleeDecl->functionTypeID == functionType.id());
	CHECK(calleeDecl->parameters[0]->typeID == sym.String());
	CHECK(calleeDecl->parameters[1]->typeID == sym.Int());
	CHECK(calleeDecl->parameters[2]->typeID == sym.Bool());
	
	auto* caller = downCast<FunctionDefinition>(tu->declarations[1].get());
	REQUIRE(caller);
	
	auto* resultDecl = downCast<VariableDeclaration>(caller->body->statements[0].get());
	CHECK(resultDecl->initExpression->typeID == sym.Float());
}

TEST_CASE("Decoration of the AST with struct definition", "[sema]") {
	std::string const text = R"(

struct X {
	var i: float;
	var j: int = 0;
	fn f(x: int, y: int) -> string {}
}

)";

	auto [ast, sym] = test::produceDecoratedASTAndSymTable(text);
	
	auto* tu = downCast<TranslationUnit>(ast.get());
	auto* xDef = downCast<StructDefinition>(tu->declarations[0].get());
	CHECK(xDef->name() == "X");
	auto* iDecl = downCast<VariableDeclaration>(xDef->body->statements[0].get());
	CHECK(iDecl->name() == "i");
	CHECK(iDecl->typeID == sym.Float());
	auto* jDecl = downCast<VariableDeclaration>(xDef->body->statements[1].get());
	CHECK(jDecl->name() == "j");
	CHECK(jDecl->typeID == sym.Int());
	auto* fDef = downCast<FunctionDefinition>(xDef->body->statements[2].get());
	CHECK(fDef->name() == "f");
	// TODO: Test argument types when we properly recognize member functions as having an implicit 'this' argument
	CHECK(fDef->returnTypeID == sym.String());
}


TEST_CASE("Semantic analysis failures", "[sema]") {
	SECTION("Use of undeclared identifier") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() -> int { return x; }"), UseOfUndeclaredIdentifier);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let v: UnknownType; }"), UseOfUndeclaredIdentifier);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { 1 + x; }"), UseOfUndeclaredIdentifier);
	}
	
	SECTION("Invalid type conversion") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() -> int { return \"a string\"; }"), BadTypeConversion);
	}
	
	SECTION("Invalid function call expression") {
		std::string const a = R"(
fn callee(a: string) {}
fn caller() { callee(); }
)";
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(a), BadFunctionCall);
	
		std::string const b = R"(
fn callee(a: string) {}
fn caller() { callee(0); }
)";
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(b), BadTypeConversion);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x: float = 1; }"), BadTypeConversion);
		CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("fn f() { let x: float = 1.; }"));
	}
	
	SECTION("Invalid function redeclaration") {
		std::string const a = R"(
fn f() {}
fn f() -> int {}
)";
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(a), InvalidFunctionDeclaration);
		
		std::string const a1 = R"(
fn f() {}
fn f() {}
)";
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(a1), InvalidFunctionDeclaration);
		
// MARK: These tests will fail when we have function overloading
//		std::string const b = R"(
//fn f();
//fn f(x: int);
//)";
//		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(b), InvalidFunctionDeclaration);
//		std::string const c = R"(
//fn f(x: string);
//fn f(x: int);
//)";
//		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(c), InvalidFunctionDeclaration);
	}
	
	SECTION("Invalid variable redeclaration") {
		std::string const a = R"(
fn f(x: int) {
	let x: float;
}
)";
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable(a), InvalidRedeclaration);

		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(x: int, x: int) {}"), InvalidRedeclaration);
	}
	
	SECTION("Invalid redeclaration category") {
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
	
	SECTION("Invalid symbol reference") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f(param: UnknownID) {}"), UseOfUndeclaredIdentifier);
	}
	
	SECTION("Invalid variable declaration") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let v; }"), InvalidStatement);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x = 0; let y: x; }"), InvalidStatement);
	}
	
	SECTION("Invalid function declaration") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { fn g(); }"), parse::ParsingIssue);
	}
	
	SECTION("Invalid struct declaration") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { struct X; }"), parse::ParsingIssue);
	}
	
	SECTION("Invalid statement at struct scope") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { return 0; }"), InvalidStatement);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { 1; }"), InvalidStatement);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { 1 + 2; }"), InvalidStatement);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { if (1 > 0) {} }"), InvalidStatement);
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { while (1 > 0) {} }"), InvalidStatement);
		CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("struct X { var i: int; }"));
	}
	
	SECTION("Invalid local scope in struct") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("struct X { {} }"), InvalidStatement);
	}
	
	SECTION("Valid local scope in function") {
		CHECK_NOTHROW(test::produceDecoratedASTAndSymTable("fn f() { {} }"));
	}
	
	SECTION("Other semantic errors") {
		CHECK_THROWS_AS(test::produceDecoratedASTAndSymTable("fn f() { let x = int; }"), SemanticIssue);
	}
	
}

