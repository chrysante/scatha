#include <Catch/Catch2.hpp>

#include <string>

#include "test/Parser/SimpleParser.h"

using namespace scatha;
using namespace parse;
using namespace ast;

static ast::UniquePtr<ast::Expression> parseExpression(std::string expression) {
    auto [ast, iss] = test::parse("fn testFn() { " + expression + "; }");
    assert(iss.empty());
    auto* const tu      = utl::down_cast<ast::TranslationUnit*>(ast.get());
    auto* const testFn  = utl::down_cast<ast::FunctionDefinition*>(tu->declarations[0].get());
    auto* exprStatement = utl::down_cast<ast::ExpressionStatement*>(testFn->body->statements[0].get());
    return std::move(exprStatement->expression);
}

TEST_CASE("Parsing expressions", "[parse]") {
    SECTION("Simple Addition") {
        // clang-format off
        //
        // Expecting:
        //      add
        //     /   \
        //   "a"   "b"
        //
        // clang-format on
        ast::UniquePtr const expr = parseExpression("a + b");
        auto* add                 = downCast<BinaryExpression>(expr.get());
        REQUIRE(add->operation() == BinaryOperator::Addition);
        auto* lhs = downCast<Identifier>(add->lhs.get());
        CHECK(lhs->value() == "a");
        auto* rhs = downCast<Identifier>(add->rhs.get());
        CHECK(rhs->value() == "b");
    }

    SECTION("Simple Multiplication") {
        // clang-format off
        //
        // Expecting:
        //     mul
        //    /   \
        //  "3"   "x"
        //
        // clang-format on
        ast::UniquePtr const expr = parseExpression("3 * x");
        auto* mul                 = downCast<BinaryExpression>(expr.get());
        REQUIRE(mul->operation() == BinaryOperator::Multiplication);
        auto* lhs = downCast<IntegerLiteral>(mul->lhs.get());
        CHECK(lhs->value() == 3);
        auto* rhs = downCast<Identifier>(mul->rhs.get());
        CHECK(rhs->value() == "x");
    }

    SECTION("Associativity") {
        // clang-format off
        //
        // Expecting:
        //      add
        //     /   \
        //   "a"   mul
        //        /   \
        //      "b"   "c"
        //
        // clang-format on
        ast::UniquePtr const expr = parseExpression("a + b * c");
        auto* add                 = downCast<BinaryExpression>(expr.get());
        REQUIRE(add->operation() == BinaryOperator::Addition);
        auto* a = downCast<Identifier>(add->lhs.get());
        CHECK(a->value() == "a");
        auto* mul = downCast<BinaryExpression>(add->rhs.get());
        REQUIRE(mul->operation() == BinaryOperator::Multiplication);
        auto* b = downCast<Identifier>(mul->lhs.get());
        CHECK(b->value() == "b");
        auto* c = downCast<Identifier>(mul->rhs.get());
        CHECK(c->value() == "c");
    }

    SECTION("Parentheses") {
        // clang-format off
        //
        // Expecting:
        //        mul
        //       /   \
        //     add   "c"
        //    /   \
        //  "a"   "b"
        //
        // clang-format on
        ast::UniquePtr const expr = parseExpression("(a + b) * c");
        auto* mul                 = downCast<BinaryExpression>(expr.get());
        REQUIRE(mul->operation() == BinaryOperator::Multiplication);
        auto* add = downCast<BinaryExpression>(mul->lhs.get());
        REQUIRE(add->operation() == BinaryOperator::Addition);
        auto* a = downCast<Identifier>(add->lhs.get());
        CHECK(a->value() == "a");
        auto* b = downCast<Identifier>(add->rhs.get());
        CHECK(b->value() == "b");
        auto* c = downCast<Identifier>(mul->rhs.get());
        CHECK(c->value() == "c");
    }
}
