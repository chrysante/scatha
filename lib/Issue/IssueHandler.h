#ifndef SCATHA_ISSUE_ISSUEHANDLER_H_
#define SCATHA_ISSUE_ISSUEHANDLER_H_

#include <memory>
#include <span>

#include "Basic/Basic.h"

namespace scatha::lex {
class LexicalIssue;
}

namespace scatha::sema {
class SemanticIssue;
}

namespace scatha::issue {

enum class Fatal;
inline constexpr Fatal fatal{};

class SCATHA(API) IssueHandler {
public:
    IssueHandler();
    IssueHandler(IssueHandler&&) noexcept;
    ~IssueHandler();

    IssueHandler& operator=(IssueHandler&&) noexcept;

    void push(lex::LexicalIssue);
    void push(lex::LexicalIssue, Fatal);
    
    void push(sema::SemanticIssue);
    void push(sema::SemanticIssue, Fatal);

    std::span<lex::LexicalIssue const> lexicalIssues() const;
    
    std::span<sema::SemanticIssue const> semaIssues() const;

    bool fatal() const { return _fatal; }

private:
    void setFatal() { _fatal = true; }
    struct Impl;

private:
    std::unique_ptr<Impl> impl;
    bool _fatal = false;
};

} // namespace scatha::issue

#endif // SCATHA_ISSUE_ISSUEHANDLER_H_
