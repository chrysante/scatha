#ifndef SCATHA_ISSUE_ISSUE2_H_
#define SCATHA_ISSUE_ISSUE2_H_

#include <iosfwd>
#include <string>

#include <scatha/AST/Token.h>

namespace scatha {

/// Base class of all compilation issues
class Issue {
public:
    virtual ~Issue() = default;

    Token token() const { return tok; }

    SourceLocation sourceLocation() const { return tok.sourceLocation(); }

    std::string toString() const { return doToString(); }

    void print();

    void print(std::ostream& ostream) { doPrint(ostream); }

protected:
    explicit Issue(Token const& token): tok(token) {}

private:
    virtual void doPrint(std::ostream&) const = 0;
    virtual std::string doToString() const    = 0;

    Token tok;
};

/// Base class of all compilation errors
class Error: public Issue {
protected:
    using Issue::Issue;
};

/// Base class of all compilation warnings
class Warning: public Issue {
protected:
    using Issue::Issue;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUE2_H_
