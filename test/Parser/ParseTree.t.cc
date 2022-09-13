#include <Catch/Catch2.hpp>

#include <iostream>

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace parse;

TEST_CASE("Parse simple function") {
	std::string const text = R"(

fn mul(a: int, b: int) -> int {
	var result = a
	return result
}

)";

	CHECK_NOTHROW([&]{
		lex::Lexer l(text);
		auto tokens = l.lex();
		
		Allocator alloc;
		Parser p(tokens, alloc);
		auto* ast = p.parse();
		
		auto* const root = dynamic_cast<RootNode*>(ast);
		REQUIRE(root != nullptr);
		REQUIRE(root->nodes.size() == 1);
		
		auto* const function = dynamic_cast<FunctionDefiniton*>(root->nodes[0]);
		REQUIRE(function != nullptr);
		CHECK(function->name == "mul");
		
		REQUIRE(function->params.size() == 2);
		CHECK(function->params[0].name == "a");
		CHECK(function->params[0].type == "int");
		CHECK(function->params[1].name == "b");
		CHECK(function->params[1].type == "int");
		
		CHECK(function->returnType == "int");
		
		Block* const body = function->body;
		REQUIRE(body->statements.size() == 2);
		
		auto* const resultDecl = dynamic_cast<VariableDeclaration*>(body->statements[0]);
		REQUIRE(resultDecl != nullptr);
		CHECK(resultDecl->name == "result");
//		CHECK(resultDecl->type == "__auto__");
		CHECK(!resultDecl->isConstant);
		
		auto* const returnStatement = dynamic_cast<ReturnStatement*>(body->statements[1]);
		REQUIRE(returnStatement != nullptr);
	}());
	
}
