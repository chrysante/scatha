#ifndef SDB_MODEL_BREAKPOINTMANAGER_H_
#define SDB_MODEL_BREAKPOINTMANAGER_H_

#include <cstddef>
#include <vector>

#include <scbinutil/OpCode.h>
#include <scdis/Disassembly.h>
#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include <scathadb/Model/BreakpointPatcher.h>
#include <scathadb/Model/SourceDebugInfo.h>
#include <scathadb/Util/Messenger.h>

namespace sdb {

class Executor;

/// High-level breakpoint manager
class BreakpointManager: Transceiver {
public:
    explicit BreakpointManager(std::shared_ptr<Messenger> messenger,
                               Executor& executor,
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

private:
    scdis::IpoIndexMap const& ipoIndexMap;
    SourceDebugInfo const& sourceDebugInfo;
    utl::hashset<size_t> instBreakpointSet;
    utl::hashset<SourceLine> sourceLineBreakpointSet;
    Executor& executor;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINTMANAGER_H_
