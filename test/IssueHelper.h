#ifndef TEST_ISSUEHELPER_H_
#define TEST_ISSUEHELPER_H_

#include <concepts>
#include <optional>
#include <string_view>

#include "AST/Fwd.h"
#include "AST/Token.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Issue/IssueHandler2.h"
#include "Sema/SymbolTable.h"

namespace scatha::lex {

class LexicalIssue;

} // namespace scatha::lex

namespace scatha::parse {

class SyntaxIssue;

} // namespace scatha::parse

namespace scatha::sema {

class SemaIssue;

} // namespace scatha::sema

namespace scatha::test {

template <typename HandlerType>
struct IssueHelper {
    template <typename T>
    std::optional<T> findOnLine(ssize_t line, ssize_t col = -1) const
        requires std::is_same_v<HandlerType, issue::SemaIssueHandler>
    {
        for (auto&& issue: iss.issues()) {
            if (issue.template is<T>()) {
                T const& t         = issue.template get<T>();
                Token const& token = t.token();
                if (token.sourceLocation().line == line &&
                    (token.sourceLocation().column == col || col == (size_t)-1))
                {
                    return t;
                }
            }
        }
        return std::nullopt;
    }

    template <typename T>
    T const* findOnLine(ssize_t line, ssize_t col = -1) const
        requires(!std::is_same_v<HandlerType, issue::SemaIssueHandler>)
    {
        for (auto* issueBase: iss.errors()) {
            auto* issue = dynamic_cast<T const*>(issueBase);
            if (!issue) {
                continue;
            }
            Token const& token = issue->token();
            if (token.sourceLocation().line == line &&
                (token.sourceLocation().column == col || col == (size_t)-1))
            {
                return issue;
            }
        }
        return nullptr;
    }

    bool noneOnLine(size_t line) const {
        for (auto&& issue: iss.issues()) {
            if (issue.token().sourceLocation().line == line) {
                return false;
            }
        }
        return true;
    }

    bool empty() const { return iss.empty(); }

    HandlerType iss;
    UniquePtr<ast::AbstractSyntaxTree> ast = nullptr;
    sema::SymbolTable sym{};
};

using LexicalIssueHelper = IssueHelper<IssueHandler>;
using SyntaxIssueHelper  = IssueHelper<IssueHandler>;
using SemaIssueHelper    = IssueHelper<issue::SemaIssueHandler>;

LexicalIssueHelper getLexicalIssues(std::string_view text);
SyntaxIssueHelper getSyntaxIssues(std::string_view text);
SemaIssueHelper getSemaIssues(std::string_view text);

} // namespace scatha::test

#endif // TEST_ISSUEHELPER_H_
