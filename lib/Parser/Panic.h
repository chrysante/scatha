#ifndef SCATHA_PARSE_PANIC_H_
#define SCATHA_PARSE_PANIC_H_

#include <span>
#include <string_view>

#include <scatha/AST/Token.h>

namespace scatha::parse {

struct PanicOptions {
    TokenKind targetDelimiter = TokenKind::Semicolon;
    bool eatDelimiter         = true;
};

void panic(class TokenStream&, PanicOptions = {});

} // namespace scatha::parse

#endif // SCATHA_PARSE_PANIC_H_
