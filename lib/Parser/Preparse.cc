#include "Parser/Preparse.h"

#include <algorithm>    // for std::find
#include <string_view>

#include <utl/stack.hpp>

#include "Parser/Bracket.h"

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
    for (auto itr = tokens.begin(); itr != tokens.end(); ++itr) {
        Bracket const bracket = toBracket(*itr);
        if (bracket.type == Bracket::Type::None) {
            continue;
        }
        Token const token = *itr;
        if (bracket.side == Bracket::Side::Open) {
            bracketStack.push(bracket);
            continue;
        }
        SC_ASSERT(bracket.side == Bracket::Side::Close, "From here on we only handle closing brackets.");
        if (bracketStack.empty()) {
            /// We have a closing bracket with no matching open bracket. Remove it from the list of tokens.
            // TODO: Raise issue here!
            itr = erase(itr);
            continue;
        }
        if (bracketStack.top().type != bracket.type) {
            /// Bracket type doesn't match the last open bracket.
            /// We search the open bracket stack for a match.
            auto& container = bracketStack.container();
            auto const stackItr = std::find_if(container.rbegin(), container.rend(), [&](Bracket b) {
                return b.type == bracket.type;
            });
            if (stackItr == container.rend()) {
                /// If we can't find the matching open bracket, we just erase this one.
                // TODO: Raise issue here!
                itr = erase(itr);
                continue;
            }
            else {
                /// Insert all missing closing brackets, i.e. all brackets matchings the open ones past
                auto i = container.rbegin();
                ssize_t const count = i - container.rend();
                itr = tokens.insert(itr, utl::narrow_cast<size_t>(count), [&]{
                    return Token(toString(Bracket{ i++->type, Bracket::Side::Close }), TokenType::Punctuation, itr->sourceLocation);
                });
                itr += count;
                SC_ASSERT(*itr == token, "'itr' must still point to the same token.");
                /// Now erase the bracket stack until \p stackItr
                container.erase(stackItr.base(), container.end());
            }
        }
        
        /// In the good case we assert the invariants. All errors must be caught before this stage.
        SC_ASSERT(bracketStack.top().side == Bracket::Side::Open, "Last bracket must be opening if we are closing here. "
                                                                  "Actually all brackets in the stack must be open.");
        SC_ASSERT(bracketStack.top().type == bracket.type, "Type must match");
        bracketStack.pop();
    
    }
    auto endSourceLocation = [&]{
        auto const& lastToken = tokens.back();
        auto result = lastToken.sourceLocation;
        result.index += lastToken.id.size();
        result.column += lastToken.id.size();
        return result;
    }();
    while (!bracketStack.empty()) {
        Bracket const bracket = bracketStack.pop();
        tokens.push_back(Token(toString(bracket), TokenType::Punctuation, endSourceLocation));
    }
}

utl::vector<Token>::iterator Context::erase(utl::vector<Token>::const_iterator itr) {
    return tokens.erase(itr) - 1;
}
