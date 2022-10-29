#ifndef TEST_ISSUEHELPER_H_
#define TEST_ISSUEHELPER_H_

#include <concepts>
#include <string_view>

#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::test {

namespace internal {

template <typename T>
constexpr auto issueGetterImpl() {
    if constexpr (std::convertible_to<T, lex::LexicalIssue>) {
        return &issue::IssueHandler::lexicalIssues;
    }
    else if constexpr (std::convertible_to<T, sema::SemanticIssue>) {
        return &issue::IssueHandler::semaIssues;
    }
}

template <typename T>
inline constexpr auto IssueGetter = issueGetterImpl<T>();

}

struct IssueHelper {
    template <typename T>
    std::optional<T> findOnLine(size_t line, size_t col = static_cast<size_t>(-1)) const {
        constexpr auto getter = internal::IssueGetter<T>;
        for (auto&& issue: std::invoke(getter, iss)) {
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
        for (auto&& issue : iss.semaIssues()) {
            if (issue.token().sourceLocation.line == line) {
                return false;
            }
        }
        return true;
    }
    
    sema::SymbolTable sym;
    issue::IssueHandler iss;
};

IssueHelper getLexicalIssues(std::string_view text);

IssueHelper getSemaIssues(std::string_view text);

}


#endif // TEST_ISSUEHELPER_H_
