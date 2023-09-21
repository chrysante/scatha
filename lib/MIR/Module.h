#ifndef SCATHA_MIR_MODULE_H_
#define SCATHA_MIR_MODULE_H_

#include <vector>

#include <utl/hashmap.hpp>

#include "Common/Base.h"
#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Represents one unit of translation
/// Also acts as a constants pool
class SCTEST_API Module {
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

    std::vector<u8> const& dataSection() const { return data; }

    void setDataSection(std::vector<uint8_t> data) {
        this->data = std::move(data);
    }

private:
    /// List of all functions in the module
    List<Function> funcs;

    /// Constant pool
    utl::node_hashmap<std::pair<uint64_t, size_t>, Constant> constants;

    /// Undef constant
    std::unique_ptr<UndefValue> undef;

    /// Data section
    std::vector<uint8_t> data;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_MODULE_H_
