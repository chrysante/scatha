#include <Catch/Catch2.hpp>

#include <string>

#include "Lexer/Lexer.h"
#include "Parser/TokenStream.h"
#include "Parser/ExpressionParser.h"

using namespace scatha;
using namespace parse;

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
		
		Allocator alloc;
		ExpressionParser parser(tokens, alloc);
		Expression* expr = parser.parseExpression();
		
		auto* add = dynamic_cast<Addition*>(expr);
		REQUIRE(add != nullptr);
		auto* left = dynamic_cast<Identifier*>(add->left);
		REQUIRE(left != nullptr);
		CHECK(left->name == "a");
		auto* right = dynamic_cast<Identifier*>(add->right);
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
		
		Allocator alloc;
		ExpressionParser parser(tokens, alloc);
		Expression* expr = parser.parseExpression();
		
		auto* mul = dynamic_cast<Multiplication*>(expr);
		REQUIRE(mul != nullptr);
		auto* left = dynamic_cast<NumericLiteral*>(mul->left);
		REQUIRE(left != nullptr);
		CHECK(left->value == "3");
		auto* right = dynamic_cast<Identifier*>(mul->right);
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
		
		Allocator alloc;
		ExpressionParser parser(tokens, alloc);
		Expression* expr = parser.parseExpression();
		
		auto* add = dynamic_cast<Addition*>(expr);
		REQUIRE(add != nullptr);
		
		auto* a = dynamic_cast<Identifier*>(add->left);
		REQUIRE(a != nullptr);
		CHECK(a->name == "a");
		
		auto* mul = dynamic_cast<Multiplication*>(add->right);
		REQUIRE(mul != nullptr);
		
		auto* b = dynamic_cast<Identifier*>(mul->left);
		REQUIRE(b != nullptr);
		CHECK(b->name == "b");
		
		auto* c = dynamic_cast<Identifier*>(mul->right);
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
		
		Allocator alloc;
		ExpressionParser parser(tokens, alloc);
		Expression* expr = parser.parseExpression();
		
		auto* mul = dynamic_cast<Multiplication*>(expr);
		REQUIRE(mul != nullptr);
		
		auto* add = dynamic_cast<Addition*>(mul->left);
		REQUIRE(add != nullptr);
		
		auto* a = dynamic_cast<Identifier*>(add->left);
		REQUIRE(a != nullptr);
		CHECK(a->name == "a");
		
		auto* b = dynamic_cast<Identifier*>(add->right);
		REQUIRE(b != nullptr);
		CHECK(b->name == "b");
		
		auto* c = dynamic_cast<Identifier*>(mul->right);
		REQUIRE(c != nullptr);
		CHECK(c->name == "c");
	}
	
}
