#ifndef SCATHA_ISSUE_ISSUE2_H_
#define SCATHA_ISSUE_ISSUE2_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Parser/Token.h>

namespace scatha {

enum class IssueSeverity { Error, Warning };

/// Base class of all compilation issues
class Issue {
public:
    virtual ~Issue() = default;

    /// Source range in which this issue occurred
    SourceRange sourceRange() const { return sourceRng; }

    SourceLocation sourceLocation() const { return sourceRng.begin(); }

    /// Severity of this issue
    IssueSeverity severity() const { return sev; }

    /// Shorthand for `severity() == Error`
    bool isError() const { return severity() == IssueSeverity::Error; }

    /// Shorthand for `severity() == Warning`
    bool isWarning() const { return severity() == IssueSeverity::Warning; }

    void setSourceLocation(SourceLocation sourceLocation) {
        setSourceRange({ sourceLocation, sourceLocation });
    }

    void setSourceRange(SourceRange sourceRange) { sourceRng = sourceRange; }

    void print(std::string_view source) const;

    void print(std::string_view source, std::ostream& ostream) const;

protected:
    explicit Issue(SourceLocation sourceLoc, IssueSeverity severity):
        Issue({ sourceLoc, sourceLoc }, severity) {}

    explicit Issue(SourceRange sourceRange, IssueSeverity severity):
        sourceRng(sourceRange), sev(severity) {}

private:
    virtual void format(std::ostream&) const = 0;

    SourceRange sourceRng;
    IssueSeverity sev;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUE2_H_
