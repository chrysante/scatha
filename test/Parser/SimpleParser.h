#ifndef SCATHA_TEST_SIMPLEPARSER_H_
#define SCATHA_TEST_SIMPLEPARSER_H_

#include <utility>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Parser/TokenStream.h"

namespace scatha::test {

std::pair<UniquePtr<ast::ASTNode>, IssueHandler> parse(std::string_view text);

parser::TokenStream makeTokenStream(std::string_view text);

} // namespace scatha::test

#endif // SCATHA_TEST_SIMPLEPARSER_H_
