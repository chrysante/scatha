#ifndef SCATHA_AST_SOURCELOCATION_H_
#define SCATHA_AST_SOURCELOCATION_H_

#include <compare>
#include <iosfwd>

#include <scatha/Common/Base.h>

namespace scatha {

/// Represents a location in source code
struct SourceLocation {
    /// \Returns `true` if this object represents a valid source location
    bool valid() const { return line > 0 && column > 0; }

    i64 index        : 44 = 0;
    size_t fileIndex : 20 = 0;
    i32 line = 0, column = 0;
};

inline bool operator==(SourceLocation const& lhs,
                       SourceLocation const& rhs) noexcept {
    bool const result = lhs.index == rhs.index &&
                        lhs.fileIndex == rhs.fileIndex;
    if (result) {
        SC_ASSERT(lhs.line == rhs.line,
                  "Line number must match for the same index.");
        SC_ASSERT(lhs.column == rhs.column,
                  "Column number must match for the same index.");
    }
    return result;
}

inline std::strong_ordering operator<=>(SourceLocation const& lhs,
                                        SourceLocation const& rhs) noexcept {
    auto idx = lhs.index <=> rhs.index;
    if (idx != std::strong_ordering::equal) {
        return idx;
    }
    /// We have to do the static cast here because of a weird bug in clang where
    /// it selects `operator<=>(int, int)` when the bitfield has less than 33
    /// bits
    return static_cast<size_t>(lhs.fileIndex) <=>
           static_cast<size_t>(rhs.fileIndex);
}

SCATHA_API std::ostream& operator<<(std::ostream&, SourceLocation const&);

/// Represents a range of characters in source code as a begin/end pair of
/// source locations
class SourceRange {
public:
    SourceRange() = default;

    SourceRange(SourceLocation begin, SourceLocation end):
        _begin(begin), _end(end) {}

    SourceLocation begin() const { return _begin; }

    SourceLocation end() const { return _end; }

    bool operator==(SourceRange const& rhs) const = default;

    /// \Returns `true` if this object represents a valid source range
    bool valid() const { return _begin.valid() && _end.valid(); }

private:
    SourceLocation _begin, _end;
};

SCATHA_API SourceRange merge(SourceRange lhs, SourceRange rhs);

template <std::same_as<SourceRange>... Args>
SourceRange merge(SourceRange lhs, Args... args) {
    ((lhs = merge(lhs, args)), ...);
    return lhs;
}

} // namespace scatha

#endif // SCATHA_AST_SOURCELOCATION_H_
