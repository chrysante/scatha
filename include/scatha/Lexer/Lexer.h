// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_LEXER_LEXER_H_
#define SCATHA_LEXER_LEXER_H_

#include <string_view>

#include <utl/vector.hpp>

#include <scatha/Basic/Basic.h>
#include <scatha/Common/Token.h>
#include <scatha/Issue/IssueHandler.h>

namespace scatha::lex {

[[nodiscard]] SCATHA(API) utl::vector<Token> lex(std::string_view text, issue::LexicalIssueHandler& issueHandler);

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXER_H_
