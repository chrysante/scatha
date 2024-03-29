#ifndef SCATHA_ISSUE_ISSUEHANDLER_H_
#define SCATHA_ISSUE_ISSUEHANDLER_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <span>
#include <vector>

#include <range/v3/view.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/Issue/Issue.h>

namespace scatha {

/// Utility class to gather issues in the front-end. All compilation steps in
/// the front end accept an issue handler to submit issues to.
class SCATHA_API IssueHandler {
    /// \Returns A views over all issues
    auto issueView() const { return _issues | ToConstAddress; }

public:
    IssueHandler() = default;

    SC_MOVEONLY(IssueHandler);

    /// Add \p error to this issue handler
    void push(std::unique_ptr<Issue> issue) {
        _issues.push_back(std::move(issue));
    }

    /// Construct issue type `T` directly in this issue handler
    template <typename T, typename... Args>
        requires std::derived_from<T, Issue> &&
                 std::constructible_from<T, Args...>
    void push(Args&&... args) {
        push(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /// Erase all issues
    void clear() { _issues.clear(); }

    /// Begin iterator
    auto begin() const { return issueView().begin(); }

    /// End iterator
    auto end() const { return issueView().end(); }

    /// First issue
    Issue const& front() const { return **begin(); }

    /// Last issue
    Issue const& back() const { return **--end(); }

    /// \Returns `true` if no  issues occurred
    bool empty() const { return _issues.empty(); }

    size_t size() const { return _issues.size(); }

    /// \Returns `true` if a fatal error has occurred
    bool fatal() const { return false; /* For now */ }

    /// Issue at index \p index
    Issue const& operator[](size_t index) const { return *_issues[index]; }

    /// \Returns `true` if any errors have occured
    bool haveErrors() const;

    /// Print all issues to \p ostream
    void print(std::span<SourceFile const> source, std::ostream& ostream) const;

    /// \overload for `std::cout` as ostream
    void print(std::span<SourceFile const> source) const;

    /// Legacy interface
    void print(std::string_view source) const;

    /// \overload for `std::cout` as ostream
    void print(std::string_view source, std::ostream& ostream) const;

private:
    std::vector<std::unique_ptr<Issue>> _issues;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUEHANDLER_H_
