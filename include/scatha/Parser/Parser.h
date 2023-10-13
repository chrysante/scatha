#ifndef SCATHA_PARSER_PARSER_H_
#define SCATHA_PARSER_PARSER_H_

#include <span>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/SourceFile.h>
#include <scatha/Parser/Token.h>

namespace scatha::parser {

/// Parse source code \p source into an abstract syntax tree
/// Issues will be submitted to \p issueHandler
/// \Returns The parsed AST or `nullptr` if lexical issues were encountered
SCATHA_API UniquePtr<ast::ASTNode> parse(
    std::span<SourceFile const> sourceFiles, IssueHandler& issueHandler);

/// Legacy single file interface
SCATHA_API UniquePtr<ast::ASTNode> parse(std::string_view source,
                                         IssueHandler& issueHandler);

} // namespace scatha::parser

#endif // SCATHA_PARSER_PARSER_H_
