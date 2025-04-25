#ifndef SCATHA_PARSER_PANIC_H_
#define SCATHA_PARSER_PANIC_H_

#include <span>
#include <string_view>

#include <scatha/Parser/Token.h>

namespace scatha::parser {

struct PanicOptions {
    TokenKind targetDelimiter = TokenKind::Semicolon;
    bool eatDelimiter = true;
};

void panic(class TokenStream&, PanicOptions = {});

} // namespace scatha::parser

#endif // SCATHA_PARSER_PANIC_H_
