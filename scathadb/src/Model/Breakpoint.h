#ifndef SDB_MODEL_BREAKPOINT_H_
#define SDB_MODEL_BREAKPOINT_H_

#include <cstddef>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

namespace sdb {

class BreakpointManager;
class Disassembly;

/// Common base class of breakpoints
class Breakpoint {
public:
    virtual ~Breakpoint() = default;

    /// \Returns the binary offset this breakpoint is set on
    size_t binaryOffset() const { return binOffset; }

    /// Called when execution hits this breakpoint
    virtual void onHit() = 0;

protected:
    Breakpoint() = default;

private:
    friend class BreakpointManager;

    std::unique_ptr<Breakpoint> next;
    size_t binOffset;
};

///
class BreakpointManager {
public:
    explicit BreakpointManager(Disassembly const* disasm): disasm(disasm) {}

    ///
    void add(size_t binaryOffset, std::unique_ptr<Breakpoint> breakpoint);

    ///
    void addAtInst(size_t index, std::unique_ptr<Breakpoint> breakpoint);

    /// Erase instruction breakpoint at binary offset \p offset
    void erase(size_t binaryOffset);

    /// Erase instruction breakpoint at index \p index
    void eraseAtInst(size_t index);

    ///
    void eraseAtSource(size_t line);

    /// Erase all breakpoints
    void clear();

    /// \Returns the breakpoint at binary offset \p offset
    Breakpoint* at(size_t offset) {
        return const_cast<Breakpoint*>(std::as_const(*this).at(offset));
    }

    /// \overload
    Breakpoint const* at(size_t offset) const;

    /// \Returns the breakpoint at binary offset \p offset
    Breakpoint* atInst(size_t index) {
        return const_cast<Breakpoint*>(std::as_const(*this).atInst(index));
    }

    /// \overload
    Breakpoint const* atInst(size_t index) const;

private:
    void eraseIf(size_t offset, auto cond);

    Disassembly const* disasm;
    utl::hashmap<size_t, std::unique_ptr<Breakpoint>> breakpoints;
};

} // namespace sdb

#endif // SDB_MODEL_BREAKPOINT_H_
