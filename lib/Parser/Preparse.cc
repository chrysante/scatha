#include "Preparse.h"

#include <algorithm>    // for std::find
#include <string_view>

#include <utl/stack.hpp>

using namespace scatha;
using namespace parse;

namespace {

struct Bracket {
    enum class Type {
        None, Parenthesis, Square, Curly
    };
    
    enum class Side {
        Open, Close
    };
    
    Type type;
    Side side;
};

static Bracket toBracket(Token const& token) {
    static constexpr std::string_view brackets[] = {
        "(", ")", "[", "]", "{", "}"
    };
    ssize_t const index = (2 + std::find(std::begin(brackets), std::end(brackets), token.id) - std::end(brackets)) % 8;
    return Bracket{ Bracket::Type(index / 2), Bracket::Side(index % 2) };
}

struct Context {
    explicit Context(utl::vector<Token>& tokens): tokens(tokens) {}
    
    void run();
    
    utl::vector<Token>& tokens;
    utl::stack<Bracket, 16> bracketStack;
};

} // namespace

void parse::preparse(utl::vector<Token>& tokens) {
    Context ctx(tokens);
    ctx.run();
}


void Context::run() {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        Token const& token = tokens[i];
        Bracket const bracket = toBracket(token);
        if (bracket.type == Bracket::Type::None) {
            continue;
        }
        
    }
}
