#ifndef SDB_MODEL_BREAKPOINTPATCHER_H_
#define SDB_MODEL_BREAKPOINTPATCHER_H_

#include <span>
#include <vector>

#include <scbinutil/OpCode.h>
#include <scdis/Disassembly.h>
#include <utl/hashtable.hpp>
#include <utl/stack.hpp>

namespace sdb {

/// Low-level breakpoint installation manager.
class BreakpointPatcher {
public:
    /// Queue the breakpoint at \p ipo for temporary removal.
    /// Can be called at positions without a breakpoint. Every call must be
    /// matched by a call to `popBreakpoint()` with the same IPO value,
    /// regardless of whether a breakpoint is installed.
    void pushBreakpoint(scdis::InstructionPointerOffset ipo, bool value);

    /// Queue the temporarily removed breakpoint at \p ipo for restoration. Must
    /// not be called without a prior call to `pushBreakpoint()`.
    void popBreakpoint(scdis::InstructionPointerOffset ipo);

    /// Queue all installed breakpoints for removal.
    void removeAll();

    /// Applies all recorded modifications.
    void patchInstructionStream(uint8_t* binary);

    /// Set the binary data of the current program. This is necessary to always
    /// have a ground truth for the instruction stream of the program.
    void setProgramData(std::span<uint8_t const> progData);

    ///
    scbinutil::OpCode opcodeAt(scdis::InstructionPointerOffset ipo) const {
        return (scbinutil::OpCode)binary[ipo.value];
    }

private:
    /// Queue a breakpoint for installation at \p ipo
    void addBreakpoint(scdis::InstructionPointerOffset ipo);

    /// Queue a breakpoint for uninstallation at \p ipo
    void removeBreakpoint(scdis::InstructionPointerOffset ipo);

    utl::hashset<scdis::InstructionPointerOffset> insertQueue, removalQueue;
    utl::hashmap<scdis::InstructionPointerOffset, utl::stack<bool>> stackMap;
    std::vector<uint8_t> binary;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINTPATCHER_H_
