#ifndef SCATHA_ISSUE_ISSUE_H_
#define SCATHA_ISSUE_ISSUE_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/SourceLocation.h>
#include <scatha/Issue/IssueSeverity.h>
#include <scatha/Issue/Message.h>
#include <scatha/Issue/SourceStructure.h>

namespace scatha {

/// This class is not exposed in public headers but forward declared here for
/// the `print()` method
class SourceStructureMap;

/// Base class of all compilation issues
class SCATHA_API Issue {
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

    ///
    void setSourceLocation(SourceLocation sourceLocation) {
        setSourceRange({ sourceLocation, sourceLocation });
    }

    ///
    void setSourceRange(SourceRange sourceRange) { sourceRng = sourceRange; }

    /// Print interface meant to be used by `IssueHandler`
    void print(SourceStructureMap& sourceMap, std::ostream& ostream) const;

    /// \overload for `std::cout` as ostream
    void print(SourceStructureMap& sourceMap) const;

    /// Convenience print interface for use with single source files
    void print(std::string_view source, std::ostream& ostream) const;

    /// \overload for `std::cout` as ostream
    void print(std::string_view source) const;

protected:
    explicit Issue(SourceLocation sourceLoc, IssueSeverity severity):
        Issue({ sourceLoc, sourceLoc }, severity) {}

    explicit Issue(SourceRange sourceRange, IssueSeverity severity):
        sourceRng(sourceRange), sev(severity) {}

    /// Define the header message of this issue
    void header(IssueMessage msg) { _header = std::move(msg); }

    /// Define a solution hint for this issue
    void hint(IssueMessage msg) { _hint = std::move(msg); }

    /// Add a source highlight message
    void highlight(HighlightKind kind,
                   SourceRange position,
                   IssueMessage message) {
        highlights.push_back({ kind, position, std::move(message) });
    }

    /// Add a primary source highlight message
    void primary(SourceRange position, IssueMessage message) {
        highlight(HighlightKind::Primary, position, std::move(message));
    }

    /// Add a secondary source highlight message
    void secondary(SourceRange position, IssueMessage message) {
        highlight(HighlightKind::Secondary, position, std::move(message));
    }

private:
    virtual void format(std::ostream&) const = 0;

    SourceRange sourceRng;
    IssueSeverity sev;
    IssueMessage _header, _hint;
    std::vector<SourceHighlight> highlights;
};

} // namespace scatha

#endif // SCATHA_ISSUE_ISSUE_H_
