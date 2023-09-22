#ifndef TEST_ISSUEHELPER_H_
#define TEST_ISSUEHELPER_H_

#include <concepts>
#include <optional>
#include <string_view>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Parser/Token.h"
#include "Sema/SymbolTable.h"

namespace scatha::test {

struct IssueHelper {
    template <typename T>
    T const* findOnLine(ssize_t line) const {
        for (auto* issueBase: iss) {
            auto* issue = dynamic_cast<T const*>(issueBase);
            if (!issue) {
                continue;
            }
            auto const sourceLoc = issue->sourceLocation();
            if (sourceLoc.line == line) {
                return issue;
            }
        }
        return nullptr;
    }

    template <typename T>
    bool findOnLine(ssize_t line, typename T::Reason reason) const {
        auto* issue = findOnLine<T>(line);
        if (!issue) {
            return false;
        }
        return issue->reason() == reason;
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
    UniquePtr<ast::ASTNode> ast = nullptr;
    sema::SymbolTable sym{};
};

IssueHelper getLexicalIssues(std::string_view text);
IssueHelper getSyntaxIssues(std::string_view text);
IssueHelper getSemaIssues(std::string_view text);

} // namespace scatha::test

#endif // TEST_ISSUEHELPER_H_
