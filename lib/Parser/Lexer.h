#ifndef SCATHA_LEXER_LEXER_H_
#define SCATHA_LEXER_LEXER_H_

#include <string_view>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/Token.h>

namespace scatha::parser {

///
SCTEST_API utl::vector<Token> lex(std::string_view text,
                                  IssueHandler& issueHandler,
                                  size_t fileIndex = 0);

} // namespace scatha::parser

#endif // SCATHA_LEXER_LEXER_H_
