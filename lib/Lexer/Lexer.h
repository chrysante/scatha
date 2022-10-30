#pragma once

#ifndef SCATHA_LEXER_LEXER_H_
#define SCATHA_LEXER_LEXER_H_

#include <string_view>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/IssueHandler.h"

namespace scatha::lex {

[[nodiscard]] SCATHA(API) utl::vector<Token> lex(std::string_view text, issue::LexicalIssueHandler& issueHandler);

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXER_H_
