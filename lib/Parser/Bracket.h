// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_BRACKET_H_
#define SCATHA_PARSER_BRACKET_H_

#include <string>

#include <scatha/Basic/Basic.h>

namespace scatha {

struct Token;

} // namespace scatha

namespace scatha::parse {

struct Bracket {
    enum class Type { None, Parenthesis, Square, Curly };

    enum class Side { Open, Close };

    Type type;
    Side side;

    bool operator==(Bracket const&) const = default;
};

SCATHA_API Bracket toBracket(Token const& token);

std::string toString(Bracket bracket);

} // namespace scatha::parse

#endif // SCATHA_PARSER_BRACKET_H_
