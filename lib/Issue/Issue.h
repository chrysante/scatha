#ifndef SCATHA_ISSUE_ISSUE2_H_
#define SCATHA_ISSUE_ISSUE2_H_

#include <iosfwd>
#include <string>

#include <scatha/Parser/Token.h>

namespace scatha {

enum class IssueSeverity { Error, Warning };

/// Base class of all compilation issues
class Issue {
public:
    virtual ~Issue() = default;

    SourceRange sourceRange() const { return sourceRng; }

    SourceLocation sourceLocation() const { return sourceRng.begin(); }

    IssueSeverity severity() const { return sev; }

    void setSourceLocation(SourceLocation sourceLocation) {
        setSourceRange({ sourceLocation, sourceLocation });
    }

    void setSourceRange(SourceRange sourceRange) { sourceRng = sourceRange; }

    void print(std::string_view source);

    void print(std::string_view source, std::ostream& ostream);

protected:
    explicit Issue(SourceLocation sourceLoc, IssueSeverity severity):
        Issue({ sourceLoc, sourceLoc }, severity) {}

    explicit Issue(SourceRange sourceRange, IssueSeverity severity):
        sourceRng(sourceRange), sev(severity) {}

private:
    virtual std::string message() const = 0;

    SourceRange sourceRng;
    IssueSeverity sev;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUE2_H_
