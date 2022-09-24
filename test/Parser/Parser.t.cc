#include <Catch/Catch2.hpp>

#include <iostream>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace parse;
using namespace ast;

static auto makeAST(std::string text) {
	lex::Lexer l(text);
	auto tokens = l.lex();
	
	Parser p(tokens);
	return p.parse();
}

TEST_CASE("Parse simple function", "[parse]") {
	
	std::string const text = R"(

fn mul(a: int, b: int) -> int {
	var result = a;
	return result;
}

)";

	auto const ast = makeAST(text);
	
	auto* const tu = dynamic_cast<TranslationUnit*>(ast.get());
	REQUIRE(tu != nullptr);
	REQUIRE(tu->declarations.size() == 1);
	
	auto* const function = dynamic_cast<FunctionDefinition*>(tu->declarations[0].get());
	REQUIRE(function != nullptr);
	CHECK(function->name() == "mul");
	
	REQUIRE(function->parameters.size() == 2);
	CHECK(function->parameters[0]->name() == "a");
	CHECK(function->parameters[0]->declTypename.id == "int");
	CHECK(function->parameters[1]->name() == "b");
	CHECK(function->parameters[1]->declTypename.id == "int");
	
	CHECK(function->declReturnTypename.id == "int");
	
	Block* const body = function->body.get();
	REQUIRE(body->statements.size() == 2);
	
	auto* const resultDecl = dynamic_cast<VariableDeclaration*>(body->statements[0].get());
	REQUIRE(resultDecl != nullptr);
	CHECK(resultDecl->name() == "result");
	CHECK(resultDecl->declTypename.empty());
	CHECK(!resultDecl->isConstant);
	CHECK(dynamic_cast<Identifier*>(resultDecl->initExpression.get()));
	
	auto* const returnStatement = dynamic_cast<ReturnStatement*>(body->statements[1].get());
	REQUIRE(returnStatement != nullptr);
	
	CHECK(dynamic_cast<Identifier*>(returnStatement->expression.get()) != nullptr);
}

TEST_CASE("Parse literals", "[parse]") {
	std::string const text = R"(

fn main() -> void {
	let a = 39;
	let b = 1.2;
}

)";

	auto const ast = makeAST(text);
	
	auto* const tu = dynamic_cast<TranslationUnit*>(ast.get());
	REQUIRE(tu != nullptr);
	REQUIRE(tu->declarations.size() == 1);
	
	auto* const function = dynamic_cast<FunctionDefinition*>(tu->declarations[0].get());
	REQUIRE(function != nullptr);
	CHECK(function->name() == "main");
	
	auto* const aDecl = dynamic_cast<VariableDeclaration*>(function->body->statements[0].get());
	auto* const intLit = dynamic_cast<IntegerLiteral*>(aDecl->initExpression.get());
	CHECK(intLit->token().id == "39");
	CHECK(intLit->value == 39);
	
	auto* const bDecl = dynamic_cast<VariableDeclaration*>(function->body->statements[1].get());
	auto* const floatLit = dynamic_cast<FloatingPointLiteral*>(bDecl->initExpression.get());
	CHECK(floatLit->token().id == "1.2");
	CHECK(floatLit->value == 1.2);
}
