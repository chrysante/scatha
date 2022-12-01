#ifndef TEST_ISSUEHELPER_H_
#define TEST_ISSUEHELPER_H_

#include <concepts>
#include <optional>
#include <string_view>

#include "Common/Token.h"
#include "Issue/IssueHandler.h"
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

namespace internal {

template <typename IssueBaseType>
struct ToIssueHandler;
template <>
struct ToIssueHandler<lex::LexicalIssue> {
    using type = issue::LexicalIssueHandler;
};
template <>
struct ToIssueHandler<parse::SyntaxIssue> {
    using type = issue::SyntaxIssueHandler;
};
template <>
struct ToIssueHandler<sema::SemanticIssue> {
    using type = issue::SemaIssueHandler;
};

} // namespace internal

template <typename IssueBaseType>
struct IssueHelper {
    template <typename T>
    std::optional<T> findOnLine(ssize_t line, ssize_t col = -1) const {
        if constexpr (std::is_same_v<IssueBaseType, parse::SyntaxIssue>) {
            static_assert(std::is_same_v<T, parse::SyntaxIssue>);
            for (auto&& issue: iss.issues()) {
                auto const& token = issue.token();
                if (token.sourceLocation.line == line && (token.sourceLocation.column == col || col == (size_t)-1)) {
                    return issue;
                }
            }
        }
        else {
            for (auto&& issue: iss.issues()) {
                if (issue.template is<T>()) {
                    T const& t         = issue.template get<T>();
                    Token const& token = t.token();
                    if (token.sourceLocation.line == line && (token.sourceLocation.column == col || col == (size_t)-1))
                    {
                        return t;
                    }
                }
            }
        }
        return std::nullopt;
    }

    bool noneOnLine(size_t line) const {
        for (auto&& issue: iss.issues()) {
            if (issue.token().sourceLocation.line == line) {
                return false;
            }
        }
        return true;
    }

    bool empty() const { return iss.empty(); }
    
    using HandlerType = typename internal::ToIssueHandler<IssueBaseType>::type;
    HandlerType iss;
    ast::UniquePtr<ast::AbstractSyntaxTree> ast = nullptr;
    sema::SymbolTable sym{};
};

using LexicalIssueHelper = IssueHelper<lex::LexicalIssue>;
using SyntaxIssueHelper = IssueHelper<parse::SyntaxIssue>;
using SemaIssueHelper = IssueHelper<sema::SemanticIssue>;


LexicalIssueHelper getLexicalIssues(std::string_view text);
SyntaxIssueHelper getSyntaxIssues(std::string_view text);
SemaIssueHelper getSemaIssues(std::string_view text);

} // namespace scatha::test

#endif // TEST_ISSUEHELPER_H_
