#ifndef SDB_MODEL_BREAKPOINT_H_
#define SDB_MODEL_BREAKPOINT_H_

#include <cstddef>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

namespace sdb {

///
class BreakpointSet {
public:
    /// Add breakpoint if none is existent at binary offset \p offset or
    /// remove otherwise
    void toggle(size_t offset) {
        auto itr = set.find(offset);
        if (itr != set.end()) {
            set.erase(itr);
        }
        else {
            set.insert(offset);
        }
    }

    /// Remove the breakpoint at binary offset \p offset
    void erase(size_t offset) { set.erase(offset); }

    /// Remove all breakpoints
    void clear() { set.clear(); }

    /// \Returns `true` if a breakpoint exists at binary offset \p offset
    bool at(size_t offset) const { return set.contains(offset); }

private:
    utl::hashset<size_t> set;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINT_H_
