#ifndef SCATHA_ISSUE_ISSUE2_H_
#define SCATHA_ISSUE_ISSUE2_H_

#include <iosfwd>
#include <string>

#include <scatha/AST/Token.h>

namespace scatha {

enum class IssueSeverity { Error, Warning };

/// Base class of all compilation issues
class Issue {
public:
    virtual ~Issue() = default;

    SourceLocation sourceLocation() const { return sourceLoc; }

    IssueSeverity severity() const { return sev; }

    std::string toString() const { return doToString(); }

    void print();

    void print(std::ostream& ostream) { doPrint(ostream); }

protected:
    explicit Issue(SourceLocation sourceLoc, IssueSeverity severity):
        sourceLoc(sourceLoc), sev(severity) {}

private:
    virtual void doPrint(std::ostream&) const = 0;
    virtual std::string doToString() const    = 0;

    SourceLocation sourceLoc;
    IssueSeverity sev;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUE2_H_
