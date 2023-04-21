// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_SOURCELOCATION_H_
#define SCATHA_AST_SOURCELOCATION_H_

#include <compare>
#include <iosfwd>

#include <scatha/Common/Base.h>

namespace scatha {

struct SourceLocation {
    i64 index = 0;
    i32 line = 0, column = 0;
};

inline bool operator==(SourceLocation const& lhs,
                       SourceLocation const& rhs) noexcept {
    bool const result = lhs.index == rhs.index;
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
    return lhs.index <=> rhs.index;
}

SCATHA_API std::ostream& operator<<(std::ostream&, SourceLocation const&);

} // namespace scatha

#endif // SCATHA_AST_SOURCELOCATION_H_
