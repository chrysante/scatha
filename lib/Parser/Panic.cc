#include "Panic.h"

#include "Parser/TokenStream.h"

using namespace scatha;

void parse::panic(TokenStream& tokens, PanicOptions const options) {
    SC_ASSERT(tokens.index() < tokens.size(), "");
    if (tokens.index() == tokens.size() - 1) {
        return;
    }
    for (ssize_t numOpenBrace = 0, numOpenParan = 0, numOpenBracket = 0;
         tokens.index() < tokens.size();)
    {
        Token const& next = tokens.peek();
        if (next.id == "{") {
            ++numOpenBrace;
        }
        if (next.id == "}") {
            --numOpenBrace;
        }
        if (next.id == "(") {
            ++numOpenParan;
        }
        if (next.id == ")") {
            --numOpenParan;
        }
        if (next.id == "[") {
            ++numOpenBracket;
        }
        if (next.id == "]") {
            --numOpenBracket;
        }
        if (numOpenBrace <= 0 && numOpenParan == 0 && numOpenBracket == 0) {
            /// Here we can potentially find a stable point to continue parsing
            if (next.type == TokenType::EndOfFile) {
                return;
            }
            if (next.id == options.targetDelimiter) {
                if (options.eatDelimiter) {
                    tokens.eat();
                }
                return;
            }
            if (next.isDeclarator) {
                return;
            }
            if (next.id == "}" && numOpenBrace == -1) {
                return;
            }
        }
        tokens.eat();
    }
}
