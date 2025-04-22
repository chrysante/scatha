#ifndef SDB_MODEL_BREAKPOINTMANAGER_H_
#define SDB_MODEL_BREAKPOINTMANAGER_H_

#include <cstddef>
#include <vector>

#include <scbinutil/OpCode.h>
#include <scdis/Disassembly.h>
#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "App/Messenger.h"
#include "Model/SourceDebugInfo.h"

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

/// High-level breakpoint manager
class BreakpointManager: Transceiver {
public:
    explicit BreakpointManager(std::shared_ptr<Messenger> messenger,
                               scdis::IpoIndexMap const& ipoIndexMap,
                               SourceDebugInfo const& sourceDebugInfo);

    /// Install or remove an instruction breakpoint at \p instIndex
    void toggleInstBreakpoint(size_t instIndex);

    /// \Returns true if an instruction breakpoint is installed at \p instIndex
    bool hasInstBreakpoint(size_t instIndex) const;

    /// Install or remove a source line breakpoint at \p line
    /// \Returns `true` if a breakpoint could be added (or removed)
    bool toggleSourceLineBreakpoint(SourceLine line);

    /// \Returns true if a source line breakpoint is installed at \p line
    bool hasSourceLineBreakpoint(SourceLine line) const;

    /// Removes all installed breakpoints
    void clearAll();

    ///
    void setProgramData(std::span<uint8_t const> progData);

private:
    void install();

    scdis::IpoIndexMap const& ipoIndexMap;
    SourceDebugInfo const& sourceDebugInfo;
    utl::hashset<size_t> instBreakpointSet;
    utl::hashset<SourceLine> sourceLineBreakpointSet;
    BreakpointPatcher patcher;
    std::vector<scdis::InstructionPointerOffset> steppingBreakpoints;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINTMANAGER_H_
