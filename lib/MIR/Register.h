#ifndef SCATHA_MIR_REGISTER_H_
#define SCATHA_MIR_REGISTER_H_

#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/List.h"
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
/// `Instruction` class, as the instructions do not care wether they are using
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
///
///
class Register:
    public ListNodeOverride<Register, Value>,
    public ParentedNode<Function> {
public:
    static constexpr size_t InvalidIndex = ~size_t{ 0 };

    size_t index() const { return idx; }

    void setIndex(size_t index) { idx = index; }

protected:
    explicit Register(NodeType nodeType, size_t index = InvalidIndex):
        ListNodeOverride<Register, Value>(nodeType) {}

private:
    friend class Instruction;

    void addDef(Instruction* inst);
    void addDefImpl(Instruction* inst) {}
    void removeDef(Instruction* inst);
    void removeDefImpl(Instruction* inst) {}
    void addUser(Instruction* inst);
    void addUserImpl(Instruction* inst) {}
    void removeUser(Instruction* inst);
    void removeUserImpl(Instruction* inst) {}

    size_t idx;
};

/// Represents a virtual register used early in the backend
///
///
class VirtualRegister: public Register {
public:
    explicit VirtualRegister(): Register(NodeType::VirtualRegister) {}

    auto defs() {
        return _defs | ranges::views::transform([](auto* d) { return d; });
    }

    auto defs() const {
        return _defs | ranges::views::transform([](auto* d) { return d; });
    }

    auto uses() {
        return _users |
               ranges::views::transform([](auto& p) { return p.first; });
    }

    auto uses() const {
        return _users |
               ranges::views::transform([](auto& p) { return p.first; });
    }

    /// \Returns `true` iff. \p inst uses this register (as an argument).
    bool isUsedBy(Instruction const* inst) const {
        return _users.contains(inst);
    }

private:
    void addDefImpl(Instruction* inst);
    void removeDefImpl(Instruction* inst);
    void addUserImpl(Instruction* inst);
    void removeUserImpl(Instruction* inst);

    utl::hashset<Instruction*> _defs;
    utl::hashmap<Instruction*, size_t> _users;
};

/// Represents a register in a callee's register space.
///
///
class CalleeRegister: public Register {
public:
    explicit CalleeRegister(): Register(NodeType::CalleeRegister) {}
};

///
///
///
class RegisterSet {
public:
    explicit RegisterSet(mir::Function* F): func(F) {}

    void add(Register* reg);

    void erase(Register* reg);

    Register* at(size_t index) { return flt[index]; }

    Register const* at(size_t index) const { return flt[index]; }

    auto begin() { return list.begin(); }

    auto begin() const { return list.begin(); }

    auto end() { return list.end(); }

    auto end() const { return list.end(); }

    bool empty() const { return list.empty(); }

    size_t size() const { return flt.size(); }

    std::span<Register* const> flat() { return flt; }

    std::span<Register const* const> flat() const { return flt; }

private:
    mir::Function* func;
    List<Register> list;
    utl::vector<Register*> flt;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_REGISTER_H_
