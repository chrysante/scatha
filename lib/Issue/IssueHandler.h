// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ISSUE_ISSUEHANDLER2_H_
#define SCATHA_ISSUE_ISSUEHANDLER2_H_

#include <concepts>
#include <iosfwd>
#include <memory>
#include <vector>

#include <range/v3/view.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>

namespace scatha {

/// Utility class to gather issues in the front-end. Several compilation steps
/// accept an issue handler to submit issues to.
class SCATHA_API IssueHandler {
    /// \Returns A views over all issues
    auto issueView() const {
        return ranges::views::transform(_issues,
                                        [](auto& p) { return p.get(); });
    }

public:
    IssueHandler() = default;

    /// Add \p error to this issue handler
    void push(Issue* issue) {
        _issues.push_back(std::unique_ptr<Issue>(issue));
    }

    /// Construct issue type `T` directly in this issue handler
    template <typename T, typename... Args>
        requires std::derived_from<T, Issue> &&
                 std::constructible_from<T, Args...>
    void push(Args&&... args) {
        push(new T(std::forward<Args>(args)...));
    }

    /// Begin iterator
    auto begin() const { return issueView().begin(); }

    /// End iterator
    auto end() const { return issueView().end(); }

    /// First issue
    Issue const& front() const { return **begin(); }

    /// Last issue
    Issue const& back() const { return **--end(); }

    /// \Returns `true` iff no  issues occurred
    bool empty() const { return _issues.empty(); }

    size_t size() const { return _issues.size(); }

    /// \Returns `true` iff a fatal error has occurred
    bool fatal() const { return false; /* For now */ }

    /// Issue at index \p index
    Issue const& operator[](size_t index) const { return *_issues[index]; }

    /// Print all issues
    void print(std::string_view source);

    /// Print all issues to \p ostream
    void print(std::string_view source, std::ostream& ostream);

private:
    std::vector<std::unique_ptr<Issue>> _issues;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUEHANDLER_H_
