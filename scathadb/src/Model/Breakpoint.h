#ifndef SDB_MODEL_BREAKPOINT_H_
#define SDB_MODEL_BREAKPOINT_H_

#include <cstddef>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

namespace sdb {

class Disassembly;

///
class BreakpointSet {
public:
    explicit BreakpointSet(Disassembly const* disasm): disasm(disasm) {}

    /// Add breakpoint if none is existent at instruction index \p instIndex or
    /// remove otherwise
    void toggle(size_t instIndex);

    /// Remove the breakpoint at instruction index \p instIndex
    void erase(size_t instIndex);

    /// Remove all breakpoints
    void clear();

    /// \Returns `true` if a breakpoint exists at instruction index \p index
    bool at(size_t instIndex) const;

    /// \Returns `true` if a breakpoint exists at binary offset \p binaryOffset
    bool atOffset(size_t binaryOffset) const;

private:
    Disassembly const* disasm;
    utl::hashset<size_t> set;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINT_H_
