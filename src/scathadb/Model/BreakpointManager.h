#ifndef SDB_MODEL_BREAKPOINTMANAGER_H_
#define SDB_MODEL_BREAKPOINTMANAGER_H_

#include <cstddef>
#include <vector>

#include <scdis/Disassembly.h>
#include <utl/hashtable.hpp>
#include <utl/messenger.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

namespace sdb {

/// Low-level breakpoint installation manager.
class BreakpointPatcher {
public:
    /// Queue a breakpoint for installation at \p ipo
    void addBreakpoint(scdis::InstructionPointerOffset ipo);

    /// Queue a breakpoint for uninstallation at \p ipo
    void removeBreakpoint(scdis::InstructionPointerOffset ipo);

    /// Queue the breakpoint at \p ipo for temporary removal.
    /// Can be called at positions without a breakpoint. Every call must be
    /// matched by a call to `popBreakpoint()` with the same IPO value,
    /// regardless of whether a breakpoint is installed.
    void pushBreakpoint(scdis::InstructionPointerOffset ipo);

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

private:
    utl::hashset<scdis::InstructionPointerOffset> insertQueue, removalQueue,
        activeBreakpoints;
    utl::hashmap<scdis::InstructionPointerOffset, utl::stack<bool>> stackMap;
    std::vector<uint8_t> binary;
};

/// High-level breakpoint manager
class BreakpointManager: utl::transceiver<utl::messenger> {
public:
    explicit BreakpointManager(std::shared_ptr<utl::messenger> messenger,
                               scdis::IpoIndexMap const& ipoIndexMap);

    /// Install or remove an instruction breakpoint at \p instIndex
    void toggleInstBreakpoint(size_t instIndex);

    /// \Returns true if an instruction breakpoint is installed at \p instIndex
    bool hasInstBreakpoint(size_t instIndex) const;

    /// Removes all installed breakpoints
    void clearAll();

    ///
    void setProgramData(std::span<uint8_t const> progData);

private:
    void install();

    scdis::IpoIndexMap const& ipoIndexMap;
    utl::hashset<size_t> instBreakpointSet;
    BreakpointPatcher patcher;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINTMANAGER_H_
