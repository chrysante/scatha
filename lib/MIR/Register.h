#ifndef SCATHA_MIR_REGISTER_H_
#define SCATHA_MIR_REGISTER_H_

#include <span>
#include <vector>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/List.h"
#include "Common/Ranges.h"
#include "MIR/LiveInterval.h"
#include "MIR/Value.h"

namespace scatha::mir {

class Instruction;

/// # IDEA
/// We replace the current `Register` class by several more fine grained
/// register classes:
///
/// First we have `VirtualRegister`, which is used until register allocation and
/// SSA destruction. That means, as long as we use `VirtualRegister` we are
/// still in SSA form. Despite that, to solve transforming three address
/// instructions to two address instructions we still need to allow multiple
/// definitions. We could later add another register class named `SSARegister`
/// to really enforce the one definition rule.
///
/// Still unclear is how we represent function arguments. One solution could be,
/// that their def is the enclosing function. Another idea is to add a register
/// class `ArgumentRegister`.
///
/// Another less central register class would be `CalleeRegister`, which is also
/// *virtual* and represents a register in the register space of
/// callees. That way we can represent copies are arguments to the register
/// space of callees that we can allocate to hardware registers late in the
/// pipeline.
///
/// Lastly we have `HardwareRegister`, which represents an actual register in
/// the hardware. This could be just an index.
///
/// We could have a common interface `Register` that is used by the
/// `Instruction` class, as the instructions do not care whether they are using
/// virtual or physical registers (except maybe for phi nodes, they should only
/// use virtual registers).
///
/// `Function` however knows about the different types of registers and owns all
/// three of them. Therefore we could have a container-like class maybe called
/// `RegisterSet` that owns and provides access to the registers. `Function`
/// could then have three `RegisterSet`'s, one of virtual register, one of
/// callee registers and one of hardware registers.
///
/// We could also add a `phi` instruction to really go all in with the SSA form
/// with virtual registers. Since the CFG does not change one in machine
/// representation, we can use the normal `Instruction` class to represent phi
/// nodes, which list their arguments in the same order as the basic block lists
/// its predecessors.
///
///

/// Abstract base class of all register classes.
class Register:
    public ListNodeOverride<Register, Value>,
    public ParentedNode<Function> {
public:
    template <typename R>
    using Override = ListNodeOverride<R, Register>;

    static constexpr size_t InvalidIndex = ~size_t{ 0 };

    Register(Register const&) = delete;

    size_t index() const { return idx; }

    void setIndex(size_t index) { idx = index & 0x7FFFFFFFFFFFFFFFu; }

    /// A register is _fixed_ if its index has a special meaning and may not be
    /// replaced by another register with a different index.
    bool fixed() const { return _fixed; }

    /// Set whether this register is fixed.
    void setFixed(bool value = true) { _fixed = value; }

    /// \Returns A view over pointers to instructions reading from this register
    auto uses() { return _users | ranges::views::keys; }

    /// \overload
    auto uses() const { return _users | ranges::views::keys; }

    /// \Returns A view over pointers to instructions writing to this register
    auto defs() { return _defs | Opaque; }

    /// \overload
    auto defs() const { return _defs | Opaque; }

    /// \Returns `true` if. \p inst uses this register (as an argument).
    bool isUsedBy(Instruction const* inst) const {
        return _users.contains(inst);
    }

    /// Replace all defs of this register with the register \p repl
    void replaceDefsWith(Register* repl);

    /// Replace all uses of this register with the register \p repl
    void replaceUsesWith(Register* repl);

    /// Replace all uses and defs of this register with the register \p repl
    void replaceWith(Register* repl);

    /// \Returns the (sorted) list of intervals where this register is live
    std::span<LiveInterval const> liveRange() const { return _liveRange; }

    /// \Returns the live interval that contains \p programPoint if such an
    /// interval exists
    std::optional<LiveInterval> liveIntervalAt(int programPoint);

    /// Adds the live interval \p I
    void addLiveInterval(LiveInterval I);

    /// Removes the live interval \p I
    /// \pre Requires \p I to be a live interval of this register
    void removeLiveInterval(LiveInterval I);

    /// Replaces the live interval \p orig by \p repl
    /// \pre Requires \p orig to be a live interval of this register
    void replaceLiveInterval(LiveInterval orig, LiveInterval repl);

    /// Sets the live range of this register
    void setLiveRange(std::vector<LiveInterval> liveRange) {
        _liveRange = std::move(liveRange);
    }

protected:
    explicit Register(NodeType nodeType, size_t index = InvalidIndex):
        ListNodeOverride<Register, Value>(nodeType) {}

private:
    friend class Instruction;
    friend class SSARegister;

    void addDef(Instruction* inst);
    void addDefImpl(Instruction* inst);
    void removeDef(Instruction* inst);
    void removeDefImpl(Instruction* inst);
    void addUser(Instruction* inst);
    void addUserImpl(Instruction* inst);
    void removeUser(Instruction* inst);
    void removeUserImpl(Instruction* inst);

    size_t idx  : 63;
    bool _fixed : 1 = false;
    utl::hashset<Instruction*> _defs;
    utl::hashmap<Instruction*, size_t> _users;
    std::vector<LiveInterval> _liveRange;
};

/// Represents a register that can only be assigned once.
class SSARegister: public Register::Override<SSARegister> {
public:
    SSARegister(): Register::Override<SSARegister>(NodeType::SSARegister) {}

    /// \Returns A pointer to the instruction defining this register.
    Instruction* def() {
        return const_cast<Instruction*>(
            static_cast<SSARegister const*>(this)->def());
    }

    /// \overload
    Instruction const* def() const {
        auto d = defs();
        return d.empty() ? nullptr : d.front();
    }

private:
    friend class Register;
    void addDefImpl(Instruction* inst);
};

/// Represents a virtual register used early in the backend
class VirtualRegister: public Register::Override<VirtualRegister> {
public:
    explicit VirtualRegister():
        Register::Override<VirtualRegister>(NodeType::VirtualRegister) {}
};

/// Represents a register in a callee's register space.
class CalleeRegister: public Register::Override<CalleeRegister> {
public:
    explicit CalleeRegister();
};

/// Represents an actual register in the hardware (or the VM)
class HardwareRegister: public Register::Override<HardwareRegister> {
public:
    HardwareRegister():
        Register::Override<HardwareRegister>(NodeType::HardwareRegister) {}
};

} // namespace scatha::mir

#endif // SCATHA_MIR_REGISTER_H_
