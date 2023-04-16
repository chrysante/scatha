#ifndef SCATHA_MIR_MODULE_H_
#define SCATHA_MIR_MODULE_H_

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Represents one unit of translation
/// Also acts as a constants pool
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

    /// Add a function to this translation unit
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

    template <typename M = Module>
    Function& front() {
        return asDerived<M>(*this).funcs.front();
    }

    template <typename M = Module const>
    Function const& front() const {
        return asDerived<M>(*this).funcs.front();
    }

    template <typename M = Module>
    Function& back() {
        return asDerived<M>(*this).funcs.back();
    }

    template <typename M = Module const>
    Function const& back() const {
        return asDerived<M>(*this).funcs.back();
    }

    Constant* constant(uint64_t value, size_t bytewidth);

    UndefValue* undefValue() const { return undef.get(); }

private:
    List<Function> funcs;
    utl::node_hashmap<std::pair<uint64_t, size_t>, Constant> constants;
    std::unique_ptr<UndefValue> undef;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_MODULE_H_
