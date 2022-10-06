#include <Catch/Catch2.hpp>

#include <string>

#include "Lexer/Lexer.h"
#include "Parser/ExpressionParser.h"
#include "Parser/TokenStream.h"

using namespace scatha;
using namespace parse;
using namespace ast;

static TokenStream makeTokenStream(std::string text) {
    lex::Lexer l(text);
    auto       tokens = l.lex();
    return TokenStream(tokens);
}

TEST_CASE("ExpressionParser", "[parse]") {
    SECTION("Simple Addition") {
        auto tokens = makeTokenStream("a + b");
        /* Expecting:

              add
             /   \
           "a"   "b"

         */

        ExpressionParser parser(tokens);
        auto             expr = parser.parseExpression();

        auto            *add  = downCast<BinaryExpression>(expr.get());
        REQUIRE(add->op == BinaryOperator::Addition);
        auto *lhs = downCast<Identifier>(add->lhs.get());
        CHECK(lhs->value() == "a");
        auto *rhs = downCast<Identifier>(add->rhs.get());
        CHECK(rhs->value() == "b");
    }

    SECTION("Simple Multiplication") {
        auto tokens = makeTokenStream("3 * x");
        /* Expecting:

                  mul
                 /   \
           "3"   "x"

         */

        ExpressionParser parser(tokens);
        auto             expr = parser.parseExpression();

        auto            *mul  = downCast<BinaryExpression>(expr.get());
        REQUIRE(mul->op == BinaryOperator::Multiplication);
        auto *lhs = downCast<IntegerLiteral>(mul->lhs.get());
        CHECK(lhs->value == 3);
        auto *rhs = downCast<Identifier>(mul->rhs.get());
        CHECK(rhs->value() == "x");
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
        auto             expr = parser.parseExpression();

        auto            *add  = downCast<BinaryExpression>(expr.get());
        REQUIRE(add->op == BinaryOperator::Addition);

        auto *a = downCast<Identifier>(add->lhs.get());
        CHECK(a->value() == "a");

        auto *mul = downCast<BinaryExpression>(add->rhs.get());
        REQUIRE(mul->op == BinaryOperator::Multiplication);

        auto *b = downCast<Identifier>(mul->lhs.get());
        CHECK(b->value() == "b");

        auto *c = downCast<Identifier>(mul->rhs.get());
        CHECK(c->value() == "c");
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
        auto             expr = parser.parseExpression();

        auto            *mul  = downCast<BinaryExpression>(expr.get());
        REQUIRE(mul->op == BinaryOperator::Multiplication);

        auto *add = downCast<BinaryExpression>(mul->lhs.get());
        REQUIRE(add->op == BinaryOperator::Addition);

        auto *a = downCast<Identifier>(add->lhs.get());
        CHECK(a->value() == "a");

        auto *b = downCast<Identifier>(add->rhs.get());
        CHECK(b->value() == "b");

        auto *c = downCast<Identifier>(mul->rhs.get());
        CHECK(c->value() == "c");
    }
}
