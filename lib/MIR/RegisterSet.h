#ifndef SCATHA_MIR_REGISTERSET_H_
#define SCATHA_MIR_REGISTERSET_H_

#include <span>

#include <utl/vector.hpp>

#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Set of registers used by function to store registers of one kind
template <std::derived_from<Register> R>
class RegisterSet {
public:
    /// Construct a register set for a function
    explicit RegisterSet(mir::Function* F): func(F) {}

    /// Add the register \p reg to this set
    void add(R* reg) {
        reg->setIndex(flt.size());
        reg->set_parent(func);
        list.push_back(reg);
        flt.push_back(reg);
    }

    /// Erase the register \p reg from this set
    void erase(R* reg) {
        flt[reg->index()] = nullptr;
        list.erase(reg);
    }

    /// Clears all registers from this set
    void clear() {
        list.clear();
        flt.clear();
    }

    /// Range accessors @{
    R* at(size_t index) { return flt[index]; }
    R const* at(size_t index) const { return flt[index]; }
    auto begin() { return list.begin(); }
    auto begin() const { return list.begin(); }
    auto end() { return list.end(); }
    auto end() const { return list.end(); }
    bool empty() const { return list.empty(); }
    size_t size() const { return flt.size(); }
    /// @}

    /// \Returns a flat view over the registers
    std::span<R* const> flat() { return flt; }

    /// \overload
    std::span<R const* const> flat() const { return flt; }

private:
    mir::Function* func;
    List<R> list;
    utl::vector<R*> flt;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_REGISTERSET_H_
