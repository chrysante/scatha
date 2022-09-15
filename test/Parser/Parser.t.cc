#include <Catch/Catch2.hpp>

#include <iostream>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace parse;
using namespace ast;

TEST_CASE("Parse simple function") {
	
	std::string const text = R"(

fn mul(a: int, b: int) -> int {
	var result = a;
	return result;
}

)";

	CHECK_NOTHROW([&]{
		lex::Lexer l(text);
		auto tokens = l.lex();
		
		Parser p(tokens);
		auto ast = p.parse();
		
		auto* const tu = dynamic_cast<TranslationUnit*>(ast.get());
		REQUIRE(tu != nullptr);
		REQUIRE(tu->declarations.size() == 1);
		
		auto* const function = dynamic_cast<FunctionDefinition*>(tu->declarations[0].get());
		REQUIRE(function != nullptr);
		CHECK(function->name() == "mul");
		
		REQUIRE(function->parameters.size() == 2);
		CHECK(function->parameters[0]->name() == "a");
		CHECK(function->parameters[0]->declTypename == "int");
		CHECK(function->parameters[1]->name() == "b");
		CHECK(function->parameters[1]->declTypename == "int");
		
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
	}());
	
}

TEST_CASE() {
//	std::string const text = R"(
//
//fn mul(a: int, b: int) -> int {
//	var result = +f(1, 2 + 3)
//}
//
//)";
//	
////	var result = a * b  + -c
////	var result = a + b % c
////	var result = f(1, 2 + 3)
////	var result = f(1, 2 + 3)
////	let x = a * (b + c)
////	let x: int = -y
////	let x
////
////	return result
//
////	CHECK_NOTHROW([&]{
//	lex::Lexer l(text);
//	auto tokens = l.lex();
//	
//	Allocator alloc;
//	Parser p(tokens, alloc);
//	auto const* const ast = p.parse();
//
//	std::cout << *ast << std::endl;
	
//	}());
	
}
