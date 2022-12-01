#ifndef PARSER_BRACKET_H_
#define PARSER_BRACKET_H_

#include "Basic/Basic.h"

namespace scatha {

struct Token;

} // namespace scatha

namespace scatha::parse {

struct Bracket {
    enum class Type {
        None, Parenthesis, Square, Curly
    };
    
    enum class Side {
        Open, Close
    };
    
    Type type;
    Side side;
    
    bool operator==(Bracket const&) = default;
};

SCATHA(API) Bracket toBracket(Token const& token);

std::string toString(Bracket bracket);
	
} // namespace scatha::parse

#endif // PARSER_BRACKET_H_

