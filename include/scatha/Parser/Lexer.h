#ifndef SCATHA_PARSER_LEXER_H_
#define SCATHA_PARSER_LEXER_H_

#include <string_view>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/Token.h>

namespace scatha::parser {

///
SCTEST_API std::vector<Token> lex(std::string_view text,
                                  IssueHandler& issueHandler,
                                  size_t fileIndex = 0);

} // namespace scatha::parser

#endif // SCATHA_PARSER_LEXER_H_
