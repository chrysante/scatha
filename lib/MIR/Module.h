#ifndef SCATHA_MIR_MODULE_H_
#define SCATHA_MIR_MODULE_H_

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

///
///
///
class SCATHA_TESTAPI Module {
    template <typename T>
    static decltype(auto) asDerived(auto&& self) {
        return static_cast<T&>(self);
    }

public:
    Module();

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

    UndefValue* undefValue() const { return undef.get(); }

private:
    List<Function> funcs;
    utl::node_hashmap<uint64_t, Constant> constants;
    std::unique_ptr<UndefValue> undef;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_MODULE_H_
