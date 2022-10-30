#ifndef TEST_ISSUEHELPER_H_
#define TEST_ISSUEHELPER_H_

#include <concepts>
#include <string_view>

#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::lex { class LexicalIssue; }
namespace scatha::parse { class ParsingIssue; }
namespace scatha::sema { class SemaIssue; }

namespace scatha::test {

namespace internal {

template <typename IssueBaseType>
struct ToIssueHandler;
template <> struct ToIssueHandler<lex::LexicalIssue> { using type = issue::LexicalIssueHandler; };
template <> struct ToIssueHandler<parse::ParsingIssue> { using type = issue::ParsingIssueHandler; };
template <> struct ToIssueHandler<sema::SemanticIssue> { using type = issue::SemaIssueHandler; };

}

template <typename IssueBaseType>
struct IssueHelper {
    template <typename T>
    std::optional<T> findOnLine(size_t line, size_t col = static_cast<size_t>(-1)) const {
        for (auto&& issue: iss.issues()) {
            if (issue.template is<T>()) {
                T const& t         = issue.template get<T>();
                Token const& token = t.token();
                if (token.sourceLocation.line == line && (token.sourceLocation.column == col || col == (size_t)-1)) {
                    return t;
                }
            }
        }
        return std::nullopt;
    }
    
    bool noneOnLine(size_t line) const {
        for (auto&& issue : iss.issues()) {
            if (issue.token().sourceLocation.line == line) {
                return false;
            }
        }
        return true;
    }
    
    sema::SymbolTable sym;
    using HandlerType = typename internal::ToIssueHandler<IssueBaseType>::type;
    HandlerType iss;
};

IssueHelper<lex::LexicalIssue> getLexicalIssues(std::string_view text);

IssueHelper<sema::SemanticIssue> getSemaIssues(std::string_view text);

}


#endif // TEST_ISSUEHELPER_H_
