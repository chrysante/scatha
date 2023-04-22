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

namespace scatha::test {

struct IssueHelper {
    template <typename T>
    T const* findOnLine(ssize_t line, ssize_t col = -1) const {
        for (auto* issueBase: iss) {
            auto* issue = dynamic_cast<T const*>(issueBase);
            if (!issue) {
                continue;
            }
            auto const sourceLoc = issue->sourceLocation();
            if (sourceLoc.line == line &&
                (sourceLoc.column == col || col == (size_t)-1))
            {
                return issue;
            }
        }
        return nullptr;
    }

    bool noneOnLine(size_t line) const {
        for (auto* issue: iss) {
            if (issue->sourceLocation().line == line) {
                return false;
            }
        }
        return true;
    }

    bool empty() const { return iss.empty(); }

    IssueHandler iss;
    UniquePtr<ast::AbstractSyntaxTree> ast = nullptr;
    sema::SymbolTable sym{};
};

IssueHelper getLexicalIssues(std::string_view text);
IssueHelper getSyntaxIssues(std::string_view text);
IssueHelper getSemaIssues(std::string_view text);

} // namespace scatha::test

#endif // TEST_ISSUEHELPER_H_
