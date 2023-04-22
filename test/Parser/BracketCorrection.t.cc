#include <Catch/Catch2.hpp>

#include "Parser/BracketCorrection.h"
#include "Parser/Lexer.h"
#include "Parser/SyntaxIssue.h"
#include "test/IssueHelper.h"
#include "test/Parser/SimpleParser.h"

using namespace scatha;
using namespace parse;

static std::tuple<utl::vector<Token>, test::IssueHelper> correctBrackets(
    std::string_view text) {
    auto tokens = [&] {
        IssueHandler iss;
        return parse::lex(text, iss);
    }();
    IssueHandler iss;
    bracketCorrection(tokens, iss);
    return { std::move(tokens), test::IssueHelper{ std::move(iss) } };
}

TEST_CASE("BracketCorrection - No issues", "[parse][preparse]") {
    SECTION("Empty") {
        auto const [tokens, iss] = correctBrackets("");
        CHECK(iss.empty());
    }
    SECTION("Simple") {
        auto const [tokens, iss] = correctBrackets("()");
        CHECK(iss.empty());
    }
    SECTION("Complex") {
        auto const [tokens, iss] =
            correctBrackets("( x{\"Hello world!\"; []({{}{}})} *!)");
        CHECK(iss.empty());
    }
}

TEST_CASE("BracketCorrection - Missing closing brackets at end of file",
          "[parse][preparse]") {
    SECTION("1") {
        auto const [tokens, iss] = correctBrackets("(");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 1);
        auto const* issue = &issues[0];
        CHECK(dynamic_cast<ExpectedClosingBracket const*>(issue));
        REQUIRE(tokens.size() == 3); /// Accounting for EOF token.
        CHECK(tokens[1].id() == ")");
        CHECK(tokens[1].sourceLocation().index == 1);
        CHECK(tokens[1].sourceLocation().line == 1);
        CHECK(tokens[1].sourceLocation().column == 2);
    }
    SECTION("2") {
        auto const [tokens, iss] = correctBrackets("([{");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 3);
        // TODO: Check for right bracket type once we added that information to
        // SyntaxIssue
        {
            auto const* curlyIssue = &issues[0];
            CHECK(dynamic_cast<ExpectedClosingBracket const*>(curlyIssue));
            CHECK(curlyIssue->sourceLocation().column == 4);
        }
        {
            auto const* squareIssue = &issues[1];
            CHECK(dynamic_cast<ExpectedClosingBracket const*>(squareIssue));
        }
        {
            auto const* paranIssue = &issues[2];
            CHECK(dynamic_cast<ExpectedClosingBracket const*>(paranIssue));
        }

        REQUIRE(tokens.size() == 7); /// Accounting for EOF token.
        CHECK(tokens[3].id() == "}");
        CHECK(tokens[3].sourceLocation().index == 3);
        CHECK(tokens[4].id() == "]");
        CHECK(tokens[4].sourceLocation().index == 3);
        CHECK(tokens[5].id() == ")");
        CHECK(tokens[5].sourceLocation().index == 3);
    }
}

TEST_CASE("BracketCorrection - Unexpected closing brackets",
          "[parse][preparse]") {
    SECTION("1") {
        auto const [tokens, iss] = correctBrackets("-)*");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 1);
        auto const* issue = &issues[0];
        CHECK(dynamic_cast<UnexpectedClosingBracket const*>(issue));
        CHECK(issue->sourceLocation().line == 1);
        CHECK(issue->sourceLocation().column == 2);
        REQUIRE(tokens.size() == 3); // { -, *, EOF }
    }
    SECTION("1.1") {
        auto const [tokens, iss] = correctBrackets("-{(abc)xyz[]-<>} \n"
                                                   // v - This one is the issue
                                                   "  ) *");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 1);
        auto const* issue = &issues[0];
        CHECK(dynamic_cast<UnexpectedClosingBracket const*>(issue));
        CHECK(issue->sourceLocation().line == 2);
        CHECK(issue->sourceLocation().column == 3);
    }
    SECTION("2") {
        auto const [tokens, iss] = correctBrackets("-[xyz*)]abc");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 1);
        auto const* issue = &issues[0];
        CHECK(dynamic_cast<UnexpectedClosingBracket const*>(issue));
        CHECK(issue->sourceLocation().column == 7);
        REQUIRE(tokens.size() == 7); /// Accounting for EOF token
    }
    SECTION("2.1") {
        auto const [tokens, iss] = correctBrackets("[)})]");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 3);
        for (int i = 2; auto* issue: issues) {
            CHECK(dynamic_cast<UnexpectedClosingBracket const*>(issue));
            CHECK(issue->sourceLocation().column == i++);
        }
        REQUIRE(tokens.size() == 3); /// Accounting for EOF token
    }
    SECTION("3") {
        auto const [tokens, iss] = correctBrackets("({)}");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 2);
        {
            auto const* missingClosingCurly = &issues[0];
            CHECK(dynamic_cast<ExpectedClosingBracket const*>(
                missingClosingCurly));
            CHECK(missingClosingCurly->sourceLocation().column == 3);
        }
        {
            auto const* unexpectedClosingCurly = &issues[1];
            CHECK(dynamic_cast<UnexpectedClosingBracket const*>(
                unexpectedClosingCurly));
            CHECK(unexpectedClosingCurly->sourceLocation().column == 4);
        }
        REQUIRE(tokens.size() == 5);
        CHECK(tokens[0].id() == "(");
        CHECK(tokens[1].id() == "{");
        CHECK(tokens[2].id() == "}");
        CHECK(tokens[3].id() == ")");
    }
    SECTION("3.1") {
        auto const [tokens, iss] = correctBrackets("( _ { _ { _ [ _ )");
        auto const& issues       = iss.iss;
        REQUIRE(issues.size() == 3);
        for (auto* issue: issues) {
            CHECK(dynamic_cast<ExpectedClosingBracket const*>(issue));
            CHECK(issue->sourceLocation().column == 17);
        }
        REQUIRE(tokens.size() == 13);
        CHECK(tokens[0].id() == "(");
        CHECK(tokens[1].id() == "_");
        CHECK(tokens[2].id() == "{");
        CHECK(tokens[3].id() == "_");
        CHECK(tokens[4].id() == "{");
        CHECK(tokens[5].id() == "_");
        CHECK(tokens[6].id() == "[");
        CHECK(tokens[7].id() == "_");
        CHECK(tokens[8].id() == "]");
        CHECK(tokens[9].id() == "}");
        CHECK(tokens[10].id() == "}");
        CHECK(tokens[11].id() == ")");
    }
}
