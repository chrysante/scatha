#ifndef SCATHA_MIR_LIVEINTERVAL_H_
#define SCATHA_MIR_LIVEINTERVAL_H_

#include <algorithm>
#include <compare>
#include <iosfwd>
#include <span>

#include "Common/Base.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Represents a half open interval `[begin, end)` in a program
struct SCATHA_API LiveInterval {
    /// The closed (included) start of the interval
    int begin;

    /// The open (excluded) end of the interval
    int end;

    /// The register which this interval describes
    Register* reg = nullptr;

    friend bool operator==(LiveInterval const&, LiveInterval const&) = default;

    friend std::strong_ordering operator<=>(LiveInterval const& lhs,
                                            LiveInterval const& rhs) {
        return lhs.begin <=> rhs.begin;
    }
};

/// Prints the interval \p I to \p ostream
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    LiveInterval const& I);

/// \Returns
/// - 0 if `P` is in the interval `I`
/// - negative value if `P` is below the interval
/// - positive value if `P` is above the interval
SCATHA_API inline int compare(LiveInterval I, int P) {
    if (P < I.begin) {
        return -1;
    }
    if (P >= I.end) {
        return 1;
    }
    return 0;
}

/// \Returns `true` if the intervals \p I and \p J overlap
SCATHA_API inline bool overlaps(LiveInterval I, LiveInterval J) {
    return I.begin < J.end && J.begin < I.end;
}

/// \Returns the interval `[min(I.begin, J.begin), max(I.end, J.end))`
SCATHA_API inline LiveInterval merge(Register* reg, LiveInterval I,
                                     LiveInterval J) {
    return { std::min(I.begin, J.begin), std::max(I.end, J.end), reg };
}

/// \overload
SCATHA_API inline LiveInterval merge(LiveInterval I, LiveInterval J) {
    SC_ASSERT(I.reg == J.reg, "Intervals must describe the same register");
    return merge(I.reg, I, J);
}

/// \Returns the subspan of the range \p range that overlaps with the interval
/// \p I
SCATHA_API std::span<LiveInterval const> rangeOverlap(
    std::span<LiveInterval const> range, LiveInterval I);

} // namespace scatha::mir

#endif // SCATHA_MIR_LIVEINTERVAL_H_
