#ifndef SCATHA_ISSUE_ISSUEHANDLER_H_
#define SCATHA_ISSUE_ISSUEHANDLER_H_

#include <memory>
#include <span>

#include "Basic/Basic.h"

namespace scatha::lex {
class LexicalIssue;
}

namespace scatha::parse {
class ParsingIssue;
}

namespace scatha::sema {
class SemanticIssue;
}

namespace scatha::issue {

enum class Fatal;
inline constexpr Fatal fatal{};

namespace internal {

template <typename T>
class SCATHA(API) IssueHandlerBase {
public:
    IssueHandlerBase();
    IssueHandlerBase(IssueHandlerBase&&) noexcept;
    ~IssueHandlerBase();
    
    IssueHandlerBase& operator=(IssueHandlerBase&&) noexcept;
    
    void push(T const&);
    void push(T&&);
    void push(T const&, Fatal);
    void push(T&&, Fatal);
    
    std::span<T const> issues() const;
    
    bool empty() const;
    
    bool fatal() const;
    
private:
    void setFatal();
    struct Impl;
    
private:
    std::unique_ptr<Impl> impl;
};

}

class LexicalIssueHandler: public internal::IssueHandlerBase<lex::LexicalIssue> {
public:
    using internal::IssueHandlerBase<lex::LexicalIssue>::IssueHandlerBase;
};

class ParsingIssueHandler: public internal::IssueHandlerBase<parse::ParsingIssue> {
public:
    using internal::IssueHandlerBase<parse::ParsingIssue>::IssueHandlerBase;
};

class SemaIssueHandler: public internal::IssueHandlerBase<sema::SemanticIssue> {
public:
    using internal::IssueHandlerBase<sema::SemanticIssue>::IssueHandlerBase;
};

} // namespace scatha::issue

#endif // SCATHA_ISSUE_ISSUEHANDLER_H_
