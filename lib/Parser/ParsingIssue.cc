#include "Parser/ParsingIssue.h"

#include <sstream>

namespace scatha::parse {

ParsingIssue::ParsingIssue(TokenEx const& token, std::string const& message):
    ProgramIssue(token, "Parsing Issue: " + message) {}

void expectIdentifier(TokenEx const& token, [[maybe_unused]] std::string_view message) {
    if (!token.isIdentifier) {
        throw ParsingIssue(token, "Expected Identifier");
    }
}

void expectKeyword(TokenEx const& token, [[maybe_unused]] std::string_view message) {
    if (!token.isKeyword) {
        throw ParsingIssue(token, "Expected Keyword");
    }
}

void expectDeclarator(TokenEx const& token, [[maybe_unused]] std::string_view message) {
    if (!token.isKeyword || token.keywordCategory != KeywordCategory::Declarators) {
        throw ParsingIssue(token, "Expected Declarator");
    }
}

void expectSeparator(TokenEx const& token, [[maybe_unused]] std::string_view message) {
    if (!token.isSeparator) {
        throw ParsingIssue(token, "Unqualified ID. Expected ';'");
    }
}

void expectID(TokenEx const& token, std::string_view id, std::string_view message) {
    if (token.id != id) {
        std::stringstream sstr;
        sstr << "Unqualified ID. Expected '" << id << "'";
        if (!message.empty()) {
            sstr << "\n" << message;
        }
        throw ParsingIssue(token, sstr.str());
    }
}

} // namespace scatha::parse
