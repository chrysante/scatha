#include <Catch/Catch2.hpp>

#include <string>

#include "Lexer/Lexer.h"
#include "Parser/ExpressionParser.h"
#include "Parser/TokenStream.h"
#include "test/Parser/SimpleParser.h"

using namespace scatha;
using namespace parse;
using namespace ast;

TEST_CASE("ExpressionParser", "[parse]") {
    SECTION("Simple Addition") {
        auto tokens = test::makeTokenStream("a + b");
        /* clang-format off
         
         Expecting:
              add
             /   \
           "a"   "b"

         clang-format on */

        issue::ParsingIssueHandler iss;
        ExpressionParser parser(tokens, iss);
        auto expr = parser.parseExpression();
        REQUIRE(iss.empty());

        auto* add = downCast<BinaryExpression>(expr.get());
        REQUIRE(add->op == BinaryOperator::Addition);
        auto* lhs = downCast<Identifier>(add->lhs.get());
        CHECK(lhs->value() == "a");
        auto* rhs = downCast<Identifier>(add->rhs.get());
        CHECK(rhs->value() == "b");
    }

    SECTION("Simple Multiplication") {
        auto tokens = test::makeTokenStream("3 * x");
        /* clang-format off
          
         Expecting:
             mul
            /   \
          "3"   "x"
         
         clang-format on */

        issue::ParsingIssueHandler iss;
        ExpressionParser parser(tokens, iss);
        auto expr = parser.parseExpression();
        REQUIRE(iss.empty());

        auto* mul = downCast<BinaryExpression>(expr.get());
        REQUIRE(mul->op == BinaryOperator::Multiplication);
        auto* lhs = downCast<IntegerLiteral>(mul->lhs.get());
        CHECK(lhs->value == 3);
        auto* rhs = downCast<Identifier>(mul->rhs.get());
        CHECK(rhs->value() == "x");
    }

    SECTION("Associativity") {
        auto tokens = test::makeTokenStream("a + b * c");
        /* clang-format off
         
         Expecting:
              add
             /   \
           "a"   mul
                /   \
              "b"   "c"

         clang-format on */

        issue::ParsingIssueHandler iss;
        ExpressionParser parser(tokens, iss);
        auto expr = parser.parseExpression();
        REQUIRE(iss.empty());

        auto* add = downCast<BinaryExpression>(expr.get());
        REQUIRE(add->op == BinaryOperator::Addition);

        auto* a = downCast<Identifier>(add->lhs.get());
        CHECK(a->value() == "a");

        auto* mul = downCast<BinaryExpression>(add->rhs.get());
        REQUIRE(mul->op == BinaryOperator::Multiplication);

        auto* b = downCast<Identifier>(mul->lhs.get());
        CHECK(b->value() == "b");

        auto* c = downCast<Identifier>(mul->rhs.get());
        CHECK(c->value() == "c");
    }

    SECTION("Parentheses") {
        auto tokens = test::makeTokenStream("(a + b) * c");
        /* clang-format off
         
         Expecting:
                mul
               /   \
             add   "c"
            /   \
          "a"   "b"

         clang-format on */

        issue::ParsingIssueHandler iss;
        ExpressionParser parser(tokens, iss);
        auto expr = parser.parseExpression();
        REQUIRE(iss.empty());

        auto* mul = downCast<BinaryExpression>(expr.get());
        REQUIRE(mul->op == BinaryOperator::Multiplication);

        auto* add = downCast<BinaryExpression>(mul->lhs.get());
        REQUIRE(add->op == BinaryOperator::Addition);

        auto* a = downCast<Identifier>(add->lhs.get());
        CHECK(a->value() == "a");

        auto* b = downCast<Identifier>(add->rhs.get());
        CHECK(b->value() == "b");

        auto* c = downCast<Identifier>(mul->rhs.get());
        CHECK(c->value() == "c");
    }
}
