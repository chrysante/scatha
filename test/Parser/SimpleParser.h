#ifndef SCATHA_TEST_SIMPLEPARSER_H_
#define SCATHA_TEST_SIMPLEPARSER_H_

#include <utility>

#include <AST/AST.h>
#include <Issue/IssueHandler.h>
#include <Parser/TokenStream.h>

namespace scatha::test {

std::pair<ast::UniquePtr<ast::AbstractSyntaxTree>, issue::SyntaxIssueHandler> parse(std::string_view text);

parse::TokenStream makeTokenStream(std::string_view text);

} // namespace scatha::test

#endif // SCATHA_TEST_SIMPLEPARSER_H_
