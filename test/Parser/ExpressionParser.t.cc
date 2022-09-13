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
		
		auto* add = dynamic_cast<Addition*>(expr.get());
		REQUIRE(add != nullptr);
		auto* left = dynamic_cast<Identifier*>(add->left.get());
		REQUIRE(left != nullptr);
		CHECK(left->name == "a");
		auto* right = dynamic_cast<Identifier*>(add->right.get());
		REQUIRE(right != nullptr);
		CHECK(right->name == "b");
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
		
		auto* mul = dynamic_cast<Multiplication*>(expr.get());
		REQUIRE(mul != nullptr);
		auto* left = dynamic_cast<NumericLiteral*>(mul->left.get());
		REQUIRE(left != nullptr);
		CHECK(left->value == "3");
		auto* right = dynamic_cast<Identifier*>(mul->right.get());
		REQUIRE(right != nullptr);
		CHECK(right->name == "x");
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
		
		auto* add = dynamic_cast<Addition*>(expr.get());
		REQUIRE(add != nullptr);
		
		auto* a = dynamic_cast<Identifier*>(add->left.get());
		REQUIRE(a != nullptr);
		CHECK(a->name == "a");
		
		auto* mul = dynamic_cast<Multiplication*>(add->right.get());
		REQUIRE(mul != nullptr);
		
		auto* b = dynamic_cast<Identifier*>(mul->left.get());
		REQUIRE(b != nullptr);
		CHECK(b->name == "b");
		
		auto* c = dynamic_cast<Identifier*>(mul->right.get());
		REQUIRE(c != nullptr);
		CHECK(c->name == "c");
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
		
		auto* mul = dynamic_cast<Multiplication*>(expr.get());
		REQUIRE(mul != nullptr);
		
		auto* add = dynamic_cast<Addition*>(mul->left.get());
		REQUIRE(add != nullptr);
		
		auto* a = dynamic_cast<Identifier*>(add->left.get());
		REQUIRE(a != nullptr);
		CHECK(a->name == "a");
		
		auto* b = dynamic_cast<Identifier*>(add->right.get());
		REQUIRE(b != nullptr);
		CHECK(b->name == "b");
		
		auto* c = dynamic_cast<Identifier*>(mul->right.get());
		REQUIRE(c != nullptr);
		CHECK(c->name == "c");
	}
	
}
