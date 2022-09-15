#include <Catch/Catch2.hpp>

#include <string>

#include "Lexer/Lexer.h"
#include "Parser/TokenStream.h"
#include "Parser/ExpressionParser.h"

using namespace scatha;
using namespace parse;
using namespace ast;

static TokenStream makeTokenStream(std::string text) {
	lex::Lexer l(text);
	auto tokens = l.lex();
	return TokenStream(tokens);
}

TEST_CASE("ExpressionParser") {
	SECTION("Simple Addition") {
		auto tokens = makeTokenStream("a + b");
		/* Expecting:
		 
		      add
		     /   \
		   "a"   "b"
		 
		 */
		
		ExpressionParser parser(tokens);
		auto expr = parser.parseExpression();
		
		auto* add = dynamic_cast<BinaryExpression*>(expr.get());
		REQUIRE(add != nullptr);
		REQUIRE(add->op == BinaryOperator::Addition);
		auto* lhs = dynamic_cast<Identifier*>(add->lhs.get());
		REQUIRE(lhs != nullptr);
		CHECK(lhs->value == "a");
		auto* rhs = dynamic_cast<Identifier*>(add->rhs.get());
		REQUIRE(rhs != nullptr);
		CHECK(rhs->value == "b");
	}
	
	SECTION("Simple Multiplication") {
		auto tokens = makeTokenStream("3 * x");
		/* Expecting:
		 
			  mul
			 /   \
		   "3"   "x"
		 
		 */
		
		ExpressionParser parser(tokens);
		auto expr = parser.parseExpression();
		
		auto* mul = dynamic_cast<BinaryExpression*>(expr.get());
		REQUIRE(mul != nullptr);
		REQUIRE(mul->op == BinaryOperator::Multiplication);
		auto* lhs = dynamic_cast<IntegerLiteral*>(mul->lhs.get());
		REQUIRE(lhs != nullptr);
		CHECK(lhs->value == 3);
		auto* rhs = dynamic_cast<Identifier*>(mul->rhs.get());
		REQUIRE(rhs != nullptr);
		CHECK(rhs->value == "x");
	}
	
	SECTION("Associativity") {
		auto tokens = makeTokenStream("a + b * c");
		/* Expecting:
		 
			  add
			 /   \
		   "a"   mul
		        /   \
			  "b"   "c"
		 
		 */
		
		ExpressionParser parser(tokens);
		auto expr = parser.parseExpression();
		
		auto* add = dynamic_cast<BinaryExpression*>(expr.get());
		REQUIRE(add != nullptr);
		REQUIRE(add->op == BinaryOperator::Addition);
		
		auto* a = dynamic_cast<Identifier*>(add->lhs.get());
		REQUIRE(a != nullptr);
		CHECK(a->value == "a");
		
		auto* mul = dynamic_cast<BinaryExpression*>(add->rhs.get());
		REQUIRE(mul != nullptr);
		REQUIRE(mul->op == BinaryOperator::Multiplication);
		
		auto* b = dynamic_cast<Identifier*>(mul->lhs.get());
		REQUIRE(b != nullptr);
		CHECK(b->value == "b");
		
		auto* c = dynamic_cast<Identifier*>(mul->rhs.get());
		REQUIRE(c != nullptr);
		CHECK(c->value == "c");
	}
	
	SECTION("Parentheses") {
		auto tokens = makeTokenStream("(a + b) * c");
		/* Expecting:
		 
			    mul
			   /   \
		     add   "c"
		    /   \
		  "a"   "b"
		 
		 */
		
		ExpressionParser parser(tokens);
		auto expr = parser.parseExpression();
		
		auto* mul = dynamic_cast<BinaryExpression*>(expr.get());
		REQUIRE(mul != nullptr);
		REQUIRE(mul->op == BinaryOperator::Multiplication);
		
		auto* add = dynamic_cast<BinaryExpression*>(mul->lhs.get());
		REQUIRE(add != nullptr);
		REQUIRE(add->op == BinaryOperator::Addition);
		
		auto* a = dynamic_cast<Identifier*>(add->lhs.get());
		REQUIRE(a != nullptr);
		CHECK(a->value == "a");
		
		auto* b = dynamic_cast<Identifier*>(add->rhs.get());
		REQUIRE(b != nullptr);
		CHECK(b->value == "b");
		
		auto* c = dynamic_cast<Identifier*>(mul->rhs.get());
		REQUIRE(c != nullptr);
		CHECK(c->value == "c");
	}
	
}
