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

    SourceLocation sourceLocation() const { return sourceLoc; }

    IssueSeverity severity() const { return sev; }

    void setSourceLocation(SourceLocation sourceLocation) {
        sourceLoc = sourceLocation;
    }

    void print(std::string_view source);

    void print(std::string_view source, std::ostream& ostream);

protected:
    explicit Issue(SourceLocation sourceLoc, IssueSeverity severity):
        sourceLoc(sourceLoc), sev(severity) {}

private:
    virtual std::string message() const = 0;

    SourceLocation sourceLoc;
    IssueSeverity sev;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUE2_H_
