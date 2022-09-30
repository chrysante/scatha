#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Parser/ParsingIssue.h"
#include "Sema/SemanticIssue.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using namespace ast;

TEST_CASE("Registration in SymbolTable", "[sema]") {
	auto const text = R"(
fn mul(a: int, b: int, c: float) -> int {
	let result = a;
	return result;
}
)";

	auto [ast, sym] = test::produceDecoratedASTAndSymTable(text);
	
	auto const& mulID = sym.lookup("mul");
	CHECK(sym.is(mulID, SymbolCategory::OverloadSet));
	auto const& mul = sym.getOverloadSet(mulID);
	
	auto const* mulFnPtr = mul.find(std::array{ sym.Int(), sym.Int(), sym.Float() });
	REQUIRE(mulFnPtr != nullptr);
	auto const& mulFn = *mulFnPtr;
	auto const& fnType = mulFn.signature();
	
	CHECK(fnType.returnTypeID() == sym.Int());
	REQUIRE(fnType.argumentCount() == 3);
	CHECK(fnType.argumentTypeID(0) == sym.Int());
	CHECK(fnType.argumentTypeID(1) == sym.Int());
	CHECK(fnType.argumentTypeID(2) == sym.Float());
	
	auto const aID = mulFn.findID("a");
	auto const& a = sym.getVariable(aID);
	CHECK(a.typeID() == sym.Int());
	
	auto const bID = mulFn.findID("b");
	auto const& b = sym.getVariable(bID);
	CHECK(b.typeID() == sym.Int());
	
	auto const cID = mulFn.findID("c");
	auto const& c = sym.getVariable(cID);
	CHECK(c.typeID() == sym.Float());
	
	auto const resultID = mulFn.findID("result");
	auto const& result = sym.getVariable(resultID);
	CHECK(result.typeID() == sym.Int());
}

TEST_CASE("Decoration of the AST") {
	auto const text = R"(
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
	auto const text = R"(
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
//	auto const& functionType = sym.getType(computeFunctionTypeID(sym.Float(), calleeArgTypes));
//	CHECK(calleeDecl->functionTypeID == functionType.id());
	CHECK(calleeDecl->parameters[0]->typeID == sym.String());
	CHECK(calleeDecl->parameters[1]->typeID == sym.Int());
	CHECK(calleeDecl->parameters[2]->typeID == sym.Bool());
	
	auto* caller = downCast<FunctionDefinition>(tu->declarations[1].get());
	auto* resultDecl = downCast<VariableDeclaration>(caller->body->statements[0].get());
	CHECK(resultDecl->initExpression->typeID == sym.Float());
	auto* fnCallExpr = downCast<FunctionCall>(resultDecl->initExpression.get());
	auto* calleeIdentifier = downCast<Identifier>(fnCallExpr->object.get());
	
	auto const& calleeOverloadSet = sym.lookupOverloadSet("callee");
	REQUIRE(calleeOverloadSet != nullptr);
	auto* calleeFunction = calleeOverloadSet->find(std::array{ sym.String(), sym.Int(), sym.Bool() });
	REQUIRE(calleeFunction != nullptr);
	
	CHECK(calleeIdentifier->symbolID == calleeFunction->symbolID());
}

TEST_CASE("Decoration of the AST with struct definition", "[sema]") {
	auto const text = R"(
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
