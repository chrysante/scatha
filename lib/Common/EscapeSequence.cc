#include "Common/EscapeSequence.h"

#include <ostream>

/// https://en.wikipedia.org/wiki/Escape_sequences_in_C#Table_of_escape_sequences

using namespace scatha;

std::optional<char> scatha::toEscapeSequence(char c) {
    switch (c) {
    case 'a':
        return 0x07;
    case 'b':
        return 0x08;
    case 'f':
        return 0x0C;
    case 'n':
        return 0x0A;
    case 'r':
        return 0x0D;
    case 't':
        return 0x09;
    case 'v':
        return 0x0B;
    case '\\':
        return 0x5C;
    case '\'':
        return 0x27;
    case '"':
        return 0x22;
    default:
        return std::nullopt;
    }
}

std::optional<char> scatha::fromEscapeSequence(char seq) {
    switch (seq) {
    case 0x07:
        return 'a';
    case 0x08:
        return 'b';
    case 0x0C:
        return 'f';
    case 0x0A:
        return 'n';
    case 0x0D:
        return 'r';
    case 0x09:
        return 't';
    case 0x0B:
        return 'v';
    case 0x5C:
        return '\\';
    case 0x27:
        return '\'';
    case 0x22:
        return '"';
    default:
        return std::nullopt;
    }
}

void scatha::printWithEscapeSeqs(std::ostream& str, std::string_view text) {
    for (char c: text) {
        if (auto raw = fromEscapeSequence(c)) {
            str << '\\' << *raw;
        }
        else {
            str << c;
        }
    }
}
