#include "MIR/LiveInterval.h"

#include <ostream>

#include <range/v3/algorithm.hpp>

using namespace scatha;
using namespace mir;

std::ostream& mir::operator<<(std::ostream& ostream, LiveInterval const& I) {
    return ostream << "[" << I.begin << ", " << I.end << ")";
}

std::span<LiveInterval const> mir::rangeOverlap(
    std::span<LiveInterval const> range, LiveInterval I) {
    auto begin = range.begin();
    for (; begin != range.end(); ++begin) {
        if (compare(I, begin->end - 1) >= 0) {
            break;
        }
    }
    auto end = begin;
    for (; end != range.end(); ++end) {
        if (compare(I, end->begin) > 0) {
            break;
        }
    }
    return { begin, end };
}
