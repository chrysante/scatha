#ifndef SCATHA_PARSER_BRACKET_H_
#define SCATHA_PARSER_BRACKET_H_

#include <string>

#include <scatha/Common/Base.h>
#include <scatha/Parser/Token.h>

namespace scatha::parser {

struct Token;

struct Bracket {
    enum class Type { None, Paranthesis, Square, Curly };

    enum class Side { Open, Close };

    Type type;
    Side side;

    bool operator==(Bracket const&) const = default;
};

SCATHA_API Bracket toBracket(Token const& token);

std::string toString(Bracket bracket);

TokenKind toTokenKind(Bracket bracket);

} // namespace scatha::parser

#endif // SCATHA_PARSER_BRACKET_H_
