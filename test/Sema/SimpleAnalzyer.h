#ifndef SCATHA_TEST_SIMPLEANALYZER_H_
#define SCATHA_TEST_SIMPLEANALYZER_H_

#include <string_view>
#include <tuple>

#include "AST/AST.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::test {

std::tuple<ast::UniquePtr<ast::AbstractSyntaxTree>, sema::SymbolTable, issue::IssueHandler>
produceDecoratedASTAndSymTable(std::string_view text);

struct IssueHelper {
    template <typename T>
    std::optional<T> findOnLine(size_t line, size_t col = -1) const {
        for (auto&& issue : iss.semaIssues()) {
            if (issue.is<T>()) {
                T const& t         = issue.get<T>();
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

inline IssueHelper getIssues(std::string_view text) {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(text);
    return { std::move(sym), std::move(iss) };
}

} // namespace scatha::test

#endif // SCATHA_TEST_SIMPLEANALYZER_H_
