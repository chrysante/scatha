#ifndef SCATHA_IR_LEXER_H_
#define SCATHA_IR_LEXER_H_

#include <string_view>

#include "Common/Expected.h"
#include "IR/Parser/Issue.h"
#include "IR/Parser/SourceLocation.h"

namespace scatha::ir {

class Token;

class Lexer {
public:
    explicit Lexer(std::string_view text):
        i(text.data()), end(i + text.size()) {}

    Expected<Token, LexicalIssue> next();

private:
    void inc();

    char const* i;
    char const* end;
    SourceLocation loc;
};

} // namespace scatha::ir

#endif // SCATHA_IR_LEXER_H_
