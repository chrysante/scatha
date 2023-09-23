#ifndef SCATHA_ISSUE_MESSAGE_H_
#define SCATHA_ISSUE_MESSAGE_H_

/// Implementation is defined in Issue/Format.cc

#include <concepts>
#include <iosfwd>
#include <string>

#include <utl/streammanip.hpp>

#include <scatha/Common/SourceLocation.h>

namespace scatha {

///
class SCATHA_API IssueMessage {
public:
    /// Construct an empty message
    IssueMessage(): msg([](std::ostream&) {}) {}

    /// Construct message from function with signature `void(std::ostream&)`
    /// that prints the message on invocation
    template <std::invocable<std::ostream&> F>
    IssueMessage(F f): msg(std::move(f)) {}

    /// Construct message from string
    IssueMessage(std::string msg);

    /// Print message to \p ostream
    friend std::ostream& operator<<(std::ostream& ostream,
                                    IssueMessage const& msg) {
        return ostream << msg.msg;
    }

private:
    utl::vstreammanip<> msg;
};

///
struct SCATHA_API SourceHighlight {
    SourceRange position;
    IssueMessage message;
};

} // namespace scatha

#endif // SCATHA_ISSUE_MESSAGE_H_
