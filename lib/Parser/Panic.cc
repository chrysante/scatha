#include "Panic.h"

#include "Parser/TokenStream.h"

using namespace scatha;

void parse::panic(TokenStream& tokens, PanicOptions const options) {
    SC_ASSERT(tokens.index() < tokens.size(), "");

    using enum TokenKind;

    if (tokens.index() == tokens.size() - 1) {
        return;
    }
    for (ssize_t numOpenBrace = 0, numOpenParan = 0, numOpenBracket = 0;
         tokens.index() < tokens.size();)
    {
        Token const& next = tokens.peek();
        switch (next.kind()) {
        case OpenBrace:
            ++numOpenBrace;
            break;
        case CloseBrace:
            --numOpenBrace;
            break;
        case OpenParan:
            ++numOpenParan;
            break;
        case CloseParan:
            --numOpenParan;
            break;
        case OpenBracket:
            ++numOpenBracket;
            break;
        case CloseBracket:
            --numOpenBracket;
            break;
        default:
            break;
        }
        if (numOpenBrace <= 0 && numOpenParan == 0 && numOpenBracket == 0) {
            /// Here we can potentially find a stable point to continue parsing
            if (next.kind() == TokenKind::EndOfFile) {
                return;
            }
            if (next.kind() == options.targetDelimiter) {
                if (options.eatDelimiter) {
                    tokens.eat();
                }
                return;
            }
            if (isDeclarator(next.kind())) {
                return;
            }
            if (next.kind() == CloseBrace && numOpenBrace == -1) {
                return;
            }
        }
        tokens.eat();
    }
}
