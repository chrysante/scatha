#include "SymbolIssue.h"

namespace scatha::sema {

void SymbolIssue::setToken(Token tok) {
    SC_ASSERT(_token.id == tok.id, "ID mismatch");
    _token = std::move(tok);
}

InvalidScopeIssue::InvalidScopeIssue(std::string_view symbolName, ScopeKind kind):
    DefinitionIssue(Token(std::string(symbolName), TokenType::Identifier)), _kind(kind) {}

SymbolCollisionIssue::SymbolCollisionIssue(std::string_view symbolName, SymbolID existing):
    DefinitionIssue(Token(std::string(symbolName), TokenType::Identifier)), _existing(existing) {}

OverloadIssue::OverloadIssue(std::string_view symbolName, SymbolID existing, Reason reason):
    SymbolCollisionIssue(symbolName, existing), _reason(reason) {}

} // namespace scatha::sema
