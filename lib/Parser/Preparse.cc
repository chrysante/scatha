#include "Parser/Preparse.h"

#include <algorithm>    // for std::find
#include <string_view>

#include <utl/stack.hpp>

#include "Parser/Bracket.h"
#include "Parser/SyntaxIssue.h"

using namespace scatha;
using namespace parse;

/// ** We need: **
/// * Additional information to construct tokens i.e. source location of string end.
/// * Even better a generic method for inserting tokens that deduces source location.
/// * Better code representation for token attributes to reduce the need for string comparisons.

namespace {

struct Context {
    explicit Context(utl::vector<Token>& tokens, issue::SyntaxIssueHandler& iss):
        tokens(tokens), iss(iss) {}
    
    void run();
    
    [[nodiscard]] utl::vector<Token>::iterator erase(utl::vector<Token>::const_iterator);
    
    utl::vector<Token>& tokens;
    issue::SyntaxIssueHandler& iss;
    utl::stack<Bracket, 16> bracketStack;
};

} // namespace

void parse::preparse(utl::vector<Token>& tokens, issue::SyntaxIssueHandler& iss) {
    Context ctx(tokens, iss);
    ctx.run();
}

void Context::run() {
    for (auto tokenItr = tokens.begin(); tokenItr != tokens.end(); ++tokenItr) {
        Bracket const bracket = toBracket(*tokenItr);
        if (bracket.type == Bracket::Type::None) {
            continue;
        }
        if (bracket.side == Bracket::Side::Open) {
            bracketStack.push(bracket);
            continue;
        }
        SC_ASSERT(bracket.side == Bracket::Side::Close, "From here on we only handle closing brackets.");
        Token const token = *tokenItr;
        if (bracketStack.empty()) {
            /// We have a closing bracket with no matching open bracket. Remove it from the list of tokens.
            iss.push(SyntaxIssue(token, SyntaxIssue::Reason::UnexpectedClosingBracket));
            tokenItr = erase(tokenItr);
            continue;
        }
        if (bracketStack.top().type != bracket.type) {
            /// Bracket type doesn't match the last open bracket.
            /// We search the open bracket stack for a match.
            auto& stackContainer = bracketStack.container();
            auto const stackReverseItr = std::find_if(stackContainer.rbegin(), stackContainer.rend(), [&](Bracket b) {
                return b.type == bracket.type;
            });
            if (stackReverseItr == stackContainer.rend()) {
                /// If we can't find the matching open bracket, we just erase this one.
                iss.push(SyntaxIssue(token, SyntaxIssue::Reason::UnexpectedClosingBracket));
                tokenItr = erase(tokenItr);
            }
            else {
                auto const stackItr = stackReverseItr.base();
                /// Insert all missing closing brackets, i.e. all brackets matchings the open ones past
                ssize_t const count = stackContainer.end() - stackItr;
                tokenItr = tokens.insert(tokenItr, utl::narrow_cast<size_t>(count), [&, itr = stackItr]() mutable {
                    iss.push(SyntaxIssue(token, SyntaxIssue::Reason::ExpectedClosingBracket));
                    SC_ASSERT(itr != stackContainer.end(), "Out of bounds");
                    Bracket const newBracket = { itr->type, Bracket::Side::Close };
                    ++itr;
                    return Token(toString(newBracket), TokenType::Punctuation, tokenItr->sourceLocation);
                });
                tokenItr += count;
                SC_ASSERT(*tokenItr == token, "'itr' must still point to the same token.");
                /// Now erase the bracket stack until \p stackItr
                stackContainer.erase(stackItr, stackContainer.end());
                SC_ASSERT(bracketStack.top().type == bracket.type, "Make sure our types match before popping.");
                bracketStack.pop();
            }
            continue;
        }
        /// Here we enter the good case. We assert the invariants. All errors must be caught before this stage.
        SC_ASSERT(bracketStack.top().side == Bracket::Side::Open, "Last bracket must be opening if we are closing here. "
                                                                  "Actually all brackets in the stack must be open.");
        SC_ASSERT(bracketStack.top().type == bracket.type, "Type must match");
        bracketStack.pop();
    
    }
    if (bracketStack.empty()) {
        return;
    }
    Token const lastToken = tokens.back();
    while (!bracketStack.empty()) {
        Bracket const bracket = bracketStack.pop();
        iss.push(SyntaxIssue(lastToken, SyntaxIssue::Reason::ExpectedClosingBracket));
        Token newToken = Token(toString(Bracket{ bracket.type, Bracket::Side::Close }),
                               TokenType::Punctuation,
                               lastToken.sourceLocation);
        tokens.insert(tokens.end() - 1, std::move(newToken));
    }
}

utl::vector<Token>::iterator Context::erase(utl::vector<Token>::const_iterator itr) {
    return tokens.erase(itr) - 1;
}
