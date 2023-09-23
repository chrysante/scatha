#ifndef SCATHA_ISSUE_MESSAGE_H_
#define SCATHA_ISSUE_MESSAGE_H_

/// Implementation is defined in Issue/Format.cc

#include <concepts>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include <scatha/Common/SourceLocation.h>

namespace scatha {

///
class SCATHA_API IssueMessage {
public:
    /// Construct an empty message
    IssueMessage() = default;

    /// Construct message from function with signature `void(std::ostream&)`
    /// that prints the message on invocation
    template <std::invocable<std::ostream&> F>
    IssueMessage(F f): func(std::move(f)) {}

    /// Construct message from string
    IssueMessage(std::string msg);

    /// \overload
    IssueMessage(std::string_view msg): IssueMessage(std::string(msg)) {}

    /// \overload
    IssueMessage(char const* msg): IssueMessage(std::string(msg)) {}

    /// Print message to \p ostream
    friend std::ostream& operator<<(std::ostream& ostream,
                                    IssueMessage const& msg) {
        if (msg.func) {
            msg.func(ostream);
        }
        return ostream;
    }

    /// \Returns `true` if this object contains a message
    explicit operator bool() const { return func != nullptr; }

private:
    std::function<void(std::ostream&)> func;
};

enum class HighlightKind { Primary, Secondary };

///
struct SCATHA_API SourceHighlight {
    HighlightKind kind;
    SourceRange position;
    IssueMessage message;
};

} // namespace scatha

#endif // SCATHA_ISSUE_MESSAGE_H_
