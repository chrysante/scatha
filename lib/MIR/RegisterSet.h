#ifndef SCATHA_MIR_REGISTERSET_H_
#define SCATHA_MIR_REGISTERSET_H_

#include <span>

#include <utl/vector.hpp>

#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

///
///
///
template <std::derived_from<Register> R>
class RegisterSet {
public:
    explicit RegisterSet(mir::Function* F): func(F) {}

    void add(R* reg) {
        reg->setIndex(flt.size());
        reg->set_parent(func);
        list.push_back(reg);
        flt.push_back(reg);
    }

    void erase(R* reg) {
        flt[reg->index()] = nullptr;
        list.erase(reg);
    }

    R* at(size_t index) { return flt[index]; }

    R const* at(size_t index) const { return flt[index]; }

    auto begin() { return list.begin(); }

    auto begin() const { return list.begin(); }

    auto end() { return list.end(); }

    auto end() const { return list.end(); }

    bool empty() const { return list.empty(); }

    size_t size() const { return flt.size(); }

    std::span<R* const> flat() { return flt; }

    std::span<R const* const> flat() const { return flt; }

private:
    mir::Function* func;
    List<R> list;
    utl::vector<R*> flt;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_REGISTERSET_H_
