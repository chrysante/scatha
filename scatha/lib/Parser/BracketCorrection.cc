#include "Parser/BracketCorrection.h"

#include <string_view>

#include <range/v3/view.hpp>
#include <utl/stack.hpp>

#include "Parser/Bracket.h"
#include "Parser/SyntaxIssue.h"

using namespace scatha;
using namespace parser;

/// ** We need: **
/// - Additional information to construct tokens i.e. source location of string
/// end.
/// - Even better a generic method for inserting tokens that deduces source
/// location.
/// - Better code representation for token attributes to reduce the need for
/// string comparisons.

namespace {

struct Context {
    explicit Context(std::vector<Token>& tokens, IssueHandler& issues):
        tokens(tokens), issues(issues) {}

    void run();

    [[nodiscard]] std::vector<Token>::iterator
        popStackAndInsertMatchingBrackets(
            std::vector<Token>::const_iterator tokenItr,
            utl::vector<Bracket>::iterator stackItr);

    [[nodiscard]] std::vector<Token>::iterator handleOpeningBracket(
        std::vector<Token>::iterator tokenItr, Bracket bracket);
    [[nodiscard]] std::vector<Token>::iterator handleClosingBracket(
        std::vector<Token>::iterator tokenItr, Bracket bracket);

    [[nodiscard]] std::vector<Token>::iterator erase(
        std::vector<Token>::const_iterator);

    std::vector<Token>& tokens;
    IssueHandler& issues;
    utl::stack<Bracket, 16> bracketStack;
};

} // namespace

void parser::bracketCorrection(std::vector<Token>& tokens,
                               IssueHandler& issues) {
    Context ctx(tokens, issues);
    ctx.run();
}

void Context::run() {
    for (auto tokenItr = tokens.begin(); tokenItr != tokens.end(); ++tokenItr) {
        Bracket const bracket = toBracket(*tokenItr);
        if (bracket.type == Bracket::Type::None) {
            continue;
        }
        switch (bracket.side) {
        case Bracket::Side::Open:
            tokenItr = handleOpeningBracket(tokenItr, bracket);
            break;

        case Bracket::Side::Close:
            tokenItr = handleClosingBracket(tokenItr, bracket);
            break;
        }
    }
    /// After traversing the list of tokens handle all unmatched open brackets
    /// (if any).
    (void)popStackAndInsertMatchingBrackets(tokens.end() - 1,
                                            bracketStack.container().begin());
    SC_ASSERT(bracketStack.empty(), "Bracket stack must be empty in the end.");
}

[[nodiscard]] std::vector<Token>::iterator Context::handleOpeningBracket(
    std::vector<Token>::iterator tokenItr, Bracket bracket) {
    SC_ASSERT(bracket.side == Bracket::Side::Open,
              "Here on we only handle opening brackets.");
    bracketStack.push(bracket);
    return tokenItr;
}

[[nodiscard]] std::vector<Token>::iterator Context::handleClosingBracket(
    std::vector<Token>::iterator tokenItr, Bracket bracket) {
    SC_ASSERT(bracket.side == Bracket::Side::Close,
              "Here on we only handle closing brackets.");
    Token const token = *tokenItr;
    if (bracketStack.empty()) {
        /// We have a closing bracket with no matching open bracket. Remove it
        /// from the list of tokens.
        issues.push<UnexpectedClosingBracket>(token);
        tokenItr = erase(tokenItr);
        return tokenItr;
    }
    if (bracketStack.top().type != bracket.type) {
        /// Bracket type doesn't match the last open bracket.
        /// We search the open bracket stack for a match.
        auto& stackContainer = bracketStack.container();
        auto const stackReverseItr =
            std::find_if(stackContainer.rbegin(), stackContainer.rend(),
                         [&](Bracket b) { return b.type == bracket.type; });
        if (stackReverseItr == stackContainer.rend()) {
            /// If we can't find the matching open bracket, we just erase this
            /// one.
            issues.push<UnexpectedClosingBracket>(token);
            tokenItr = erase(tokenItr);
            return tokenItr;
        }
        else {
            auto const stackItr = stackReverseItr.base();
            tokenItr = popStackAndInsertMatchingBrackets(tokenItr, stackItr);
            SC_ASSERT(*tokenItr == token,
                      "'itr' must still point to the same token.");
            /// From this case we don't `return` but flow into the good case
            /// section, since we have corrected all errors but still need to
            /// pop the last open bracket.
        }
    }
    /// Here we enter the good case. We assert the invariants. All errors must
    /// be caught before this stage.
    SC_ASSERT(bracketStack.top().side == Bracket::Side::Open,
              "Last bracket must be opening if we are closing here. "
              "Actually all brackets in the stack must be open.");
    SC_ASSERT(bracketStack.top().type == bracket.type, "Type must match");
    bracketStack.pop();
    return tokenItr;
}

std::vector<Token>::iterator Context::popStackAndInsertMatchingBrackets(
    std::vector<Token>::const_iterator tokenItr,
    utl::vector<Bracket>::iterator stackItr) {
    auto& stackContainer = bracketStack.container();
    ssize_t const count = stackContainer.end() - stackItr;
    /// Insert all missing closing brackets, i.e. all brackets matchings the
    /// open ones past and push errors.
    auto insertRange = stackContainer | ranges::views::reverse |
                       ranges::views::take(count) |
                       ranges::views::transform([&](Bracket bracket) {
        issues.push<ExpectedClosingBracket>(*tokenItr);
        Bracket const newBracket = { bracket.type, Bracket::Side::Close };
        return Token(toString(newBracket), toTokenKind(newBracket),
                     tokenItr->sourceRange());
    }) | ranges::views::common;
    auto const resultItr = tokens.insert(tokenItr, ranges::begin(insertRange),
                                         ranges::end(insertRange));
    /// Now erase the bracket stack until \p stackItr
    stackContainer.erase(stackItr, stackContainer.end());
    return resultItr + count;
}

std::vector<Token>::iterator Context::erase(
    std::vector<Token>::const_iterator itr) {
    return tokens.erase(itr) - 1;
}
