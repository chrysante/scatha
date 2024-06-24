#include <catch2/catch_test_macros.hpp>

#include "AST/AST.h"
#include "Parser/SyntaxIssue.h"
#include "Util/IssueHelper.h"

using namespace scatha;
using namespace parser;

static void expectFooParse(ast::ASTNode const& ast) {
    auto const& file = *cast<ast::TranslationUnit const&>(ast).sourceFile(0);
    auto const& fooDecl = *file.statement<ast::FunctionDefinition>(0);
    CHECK(fooDecl.name() == "foo");
    CHECK(fooDecl.returnTypeExpr() == nullptr);
}

TEST_CASE("UnqualifiedID - 1", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn foo . () {}
)");
    auto issue = iss.findOnLine<UnqualifiedID>(2);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 2);
    CHECK(issue->sourceLocation().column == 8);
    /// Expect recovery from the syntax issue.
    expectFooParse(*iss.ast);
}

TEST_CASE("UnqualifiedID - 2", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn foo() . {}
)");
    auto issue = iss.findOnLine<UnqualifiedID>(2);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 2);
    CHECK(issue->sourceLocation().column == 10);
    /// Expect recovery from the syntax issue.
    expectFooParse(*iss.ast);
}

TEST_CASE("ExpectedIdentifier - 1", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn . foo() {}
)");
    auto issue = iss.findOnLine<ExpectedIdentifier>(2);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 2);
    CHECK(issue->sourceLocation().column == 4);
    /// Expect recovery from the syntax issue.
    expectFooParse(*iss.ast);
}

TEST_CASE("ExpectedDeclarator - 1", "[parse][issue]") {
    auto iss = test::getSyntaxIssues("foo");
    auto issue = iss.findOnLine<ExpectedDeclarator>(1);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 1);
    CHECK(issue->sourceLocation().column == 1);
}

TEST_CASE("ExpectedDeclarator - 2", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn foo() {} foo;
)");
    auto issue = iss.findOnLine<ExpectedDeclarator>(2);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 2);
    CHECK(issue->sourceLocation().column == 13);
    expectFooParse(*iss.ast);
}

TEST_CASE("ExpectedDeclarator - 3", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
lit i = j;
fn foo() {}
)");
    auto issue = iss.findOnLine<ExpectedDeclarator>(2);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 2);
    CHECK(issue->sourceLocation().column == 1);
    expectFooParse(*iss.ast);
}

TEST_CASE("ExpectedExpression - 1", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn foo() {
      a * ;
      a / ;
      a % ;
      a + ;
      a - ;
     a << ;
     a >> ;
      a < ;
     a <= ;
      a > ;
     a >= ;
     a == ;
     a != ;
      a & ;
      a ^ ;
      a | ;
     a && ;
     a || ;
true? a : ;
      a = ;
     a *= ;
     a /= ;
     a %= ;
     a += ;
     a -= ;
    a <<= ;
    a >>= ;
     a &= ;
     a ^= ;
     a |= ;
      a , ;
          / a;
          % a;
          << a;
          >> a;
          < a;
          <= a;
          > a;
          >= a;
          == a;
          != a;
          ^ a;
          | a;
          && a;
          || a;
          ? a : b;
          = a;
          *= a;
          /= a;
          %= a;
          += a;
          -= a;
          <<= a;
          >>= a;
          &= a;
          ^= a;
          |= a;
          , a;
         +;
         -;
         ~;
         !;
})");
    int const numLines = 62;
    for (int i = 0; i < numLines; ++i) {
        int const line = i + 3;
        auto issue = iss.findOnLine<ExpectedExpression>(line);
        REQUIRE(issue);
        CHECK(issue->sourceLocation().line == line);
        CHECK(issue->sourceLocation().column == 11);
    }
}

TEST_CASE("ExpectedExpression - 2", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn foo() {
    (;
})");
    auto issue = iss.findOnLine<ExpectedExpression>(3);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 3);
    CHECK(issue->sourceLocation().column == 6);
    expectFooParse(*iss.ast);
}

TEST_CASE("ExpectedExpression - Parameter type", "[parse][issue]") {
    auto iss = test::getSyntaxIssues("fn foo(x:) {}");
    auto issue = iss.findOnLine<ExpectedExpression>(1);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 1);
    CHECK(issue->sourceLocation().column == 10);
    expectFooParse(*iss.ast);
}

TEST_CASE("Missing parameter name", "[parse][issue]") {
    auto iss = test::getSyntaxIssues("fn foo(:x) {}");
    auto issue = iss.findOnLine<ExpectedIdentifier>(1);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 1);
    CHECK(issue->sourceLocation().column == 8);
}

TEST_CASE("Missing struct name", "[parse][issue]") {
    auto iss = test::getSyntaxIssues("struct {}");
    auto issue = iss.findOnLine<ExpectedIdentifier>(1);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().line == 1);
    CHECK(issue->sourceLocation().column == 8);
}

TEST_CASE("Unclosed fstring", "[parse][issue]") {
    auto iss = test::getSyntaxIssues(R"(
fn test() {
    "\(42;
})");
    auto issue = iss.findOnLine<ExpectedFStringEnd>(3);
    REQUIRE(issue);
    CHECK(issue->sourceLocation().column == 10);
}
