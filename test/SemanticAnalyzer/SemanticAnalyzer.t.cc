#include <Catch/Catch2.hpp>

#include <iostream>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "SemanticAnalyzer/SemanticAnalyzer.h"

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
	
	return std::tuple<UniquePtr<AbstractSyntaxTree>, SymbolTable>(std::move(ast), s.takeSymbolTable());
}

TEST_CASE("Registration in SymbolTable") {
	std::string const text = R"(

fn mul(a: int, b: int, c: float) -> int {
	let result = a;
	return result;
}

)";

	auto [ast, sym] = produceDecoratedASTAndSymTable(text);
	
	auto const& symMul = sym.lookupName("mul");
	CHECK(symMul.category() == NameCategory::Function);
	
	auto const& fnMul = sym.getFunction(symMul);
	auto const& fnType = sym.getType(fnMul.typeID());
	
	CHECK(fnType.returnType() == sym.Int());
	REQUIRE(fnType.argumentTypes().size() == 3);
	CHECK(fnType.argumentType(0) == sym.Int());
	CHECK(fnType.argumentType(1) == sym.Int());
	CHECK(fnType.argumentType(2) == sym.Float());
	
	auto const  mulScopeID = sym.globalScope()->findIDByName("mul");
	auto const* mulScope   = sym.globalScope()->childScope(mulScopeID);
	
	auto const aID      = mulScope->findIDByName("a");
	auto const& a = sym.getVariable(aID);
	CHECK(a.typeID() == sym.Int());
	
	auto const bID      = mulScope->findIDByName("b");
	auto const& b = sym.getVariable(bID);
	CHECK(b.typeID() == sym.Int());
	
	auto const cID      = mulScope->findIDByName("c");
	auto const& c = sym.getVariable(cID);
	CHECK(c.typeID() == sym.Float());
	
	
	auto const resultID = mulScope->findIDByName("result");
	auto const& result = sym.getVariable(resultID);
	CHECK(result.typeID() == sym.Int());
}

TEST_CASE("Decoration of the AST") {
	std::string const text = R"(

fn mul(a: int, b: int, c: float, d: string) -> int;

fn mul(a: int, b: int, c: float, d: string) -> int {
	let result = a;
	return result;
}

)";

	auto [ast, sym] = produceDecoratedASTAndSymTable(text);
	
	auto* tu = dynamic_cast<TranslationUnit*>(ast.get());
	REQUIRE(tu);
	auto* fnDecl = dynamic_cast<FunctionDeclaration*>(tu->declarations[0].get());
	REQUIRE(fnDecl);
	CHECK(fnDecl->returnTypeID == sym.Int());
	CHECK(fnDecl->parameters[0]->typeID == sym.Int());
	CHECK(fnDecl->parameters[1]->typeID == sym.Int());
	CHECK(fnDecl->parameters[2]->typeID == sym.Float());
	CHECK(fnDecl->parameters[3]->typeID == sym.String());
	
	auto* fn = dynamic_cast<FunctionDefinition*>(tu->declarations[1].get());
	REQUIRE(fn);
	
	CHECK(fn->returnTypeID == sym.Int());
	CHECK(fn->parameters[0]->typeID == sym.Int());
	CHECK(fn->parameters[1]->typeID == sym.Int());
	CHECK(fn->parameters[2]->typeID == sym.Float());
	CHECK(fn->parameters[3]->typeID == sym.String());
	
	auto* varDecl = dynamic_cast<VariableDeclaration*>(fn->body->statements[0].get());
	CHECK(varDecl->typeID == sym.Int());
	
	auto* varDeclInit = dynamic_cast<Identifier*>(varDecl->initExpression.get());
	CHECK(varDeclInit->typeID == sym.Int());
	
	auto* ret = dynamic_cast<ReturnStatement*>(fn->body->statements[1].get());
	auto* retIdentifier = dynamic_cast<Identifier*>(ret->expression.get());
	CHECK(retIdentifier->typeID == sym.Int());
	
}
