#ifndef SCATHA_MIR_MODULE_H_
#define SCATHA_MIR_MODULE_H_

#include <utl/hashmap.hpp>

#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

///
///
///
class Module {
    template <typename T>
    static decltype(auto) asDerived(auto&& self) {
        return static_cast<T&>(self);
    }

public:
    Module() = default;

    Module(Module const&) = delete;

    Module(Module&&) noexcept;

    Module& operator=(Module const&) = delete;

    Module& operator=(Module&&) noexcept;

    ~Module();

    void addFunction(Function* function);

    template <typename M = Module>
    auto begin() {
        return asDerived<M>(*this).funcs.begin();
    }

    template <typename M = Module const>
    auto begin() const {
        return asDerived<M>(*this).funcs.begin();
    }

    template <typename M = Module>
    auto end() {
        return asDerived<M>(*this).funcs.end();
    }

    template <typename M = Module const>
    auto end() const {
        return asDerived<M>(*this).funcs.end();
    }

    Constant* constant(uint64_t value);

private:
    List<Function> funcs;
    utl::node_hashmap<uint64_t, Constant> constants;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_MODULE_H_
