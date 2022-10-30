#pragma once

#ifndef SCATHA_PARSER_PARSER_H_
#define SCATHA_PARSER_PARSER_H_

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/IssueHandler.h"

namespace scatha::parse {

[[nodiscard]] SCATHA(API) ast::UniquePtr<ast::AbstractSyntaxTree> parse(utl::vector<Token> tokens, issue::ParsingIssueHandler& issueHandler);

} // namespace scatha::parse

#endif // SCATHA_LEXER_LEXER_H_
