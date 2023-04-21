// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ISSUE_ISSUEHANDLER2_H_
#define SCATHA_ISSUE_ISSUEHANDLER2_H_

#include <concepts>
#include <memory>
#include <span>
#include <vector>

#include <range/v3/view.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue2.h>

namespace scatha {

/// Utility class to gather issues in the front-end. Several compilation steps
/// accept an issue handler to submit issues to.
class SCATHA_API IssueHandler {
public:
    IssueHandler() = default;

    /// Add \p error to this issue handler
    void push(Error* error) { errs.push_back(std::unique_ptr<Error>(error)); }

    /// Add \p warning to this issue handler
    void push(Warning* warning) {
        warns.push_back(std::unique_ptr<Warning>(warning));
    }

    /// Construct issue type `T` directly in this issue handler
    template <typename T, typename... Args>
        requires std::derived_from<T, Issue> &&
                 std::constructible_from<T, Args...>
    void push(Args&&... args) {
        push(new T(std::forward<Args>(args)...));
    }
    
    /// \Returns A views over the errors
    auto errors() const {
        return ranges::views::transform(errs, [](auto& p) { return p.get(); });
    }

    /// \Returns A views over the warnings
    auto warnings() const {
        return ranges::views::transform(warns, [](auto& p) { return p.get(); });
    }

    /// \Returns `true` iff no  errors occured
    bool empty() const;

private:
    std::vector<std::unique_ptr<Error>> errs;
    std::vector<std::unique_ptr<Warning>> warns;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUEHANDLER_H_
