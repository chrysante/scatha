#include "Lexer/ExtractLines.h"

using namespace scatha;
using namespace lex;

utl::vector<std::string> lex::extractLines(std::string_view text) {
    utl::vector<std::string> result;
    size_t pos = 0;
    while (true) {
        size_t lineEnd = pos;
        while (lineEnd < text.size() && text[lineEnd] != '\n' && text[lineEnd] != '\r') {
            ++lineEnd;
        }
        result.push_back(std::string(text.substr(pos, lineEnd - pos)));
        if (++lineEnd >= text.size()) {
            break;
        }
        if (text[lineEnd - 1] == '\r' && lineEnd < text.size() && text[lineEnd] == '\n') {
            // Handle "\r\n" new line sequence
            ++lineEnd;
        }
        pos = lineEnd;
        if (pos >= text.size()) {
            break;
        }
    }
    return result;
}
