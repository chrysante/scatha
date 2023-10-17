#ifndef SCATHA_MIR_MODULE_H_
#define SCATHA_MIR_MODULE_H_

#include <span>
#include <vector>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

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

    /// Iterators and other helper functions to make `Module` a range of
    /// functions
    /// @{
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
    /// @}

    ///
    Constant* constant(uint64_t value, size_t bytewidth);

    ///
    UndefValue* undefValue() const { return undef.get(); }

    ///
    std::vector<u8> const& dataSection() const { return staticData; }

    /// Allocates data in the data section of the program.
    /// \Returns pair of pointer to the data and the offset from the start of
    /// the data section where this allocation is placed Returned pointer will
    /// be valid until the next call to this function
    std::pair<void*, size_t> allocateStaticData(size_t size, size_t align);

    ///
    void addAddressPlaceholder(size_t offset, Function const* function);

    ///
    std::span<std::pair<size_t, Function const*> const> addressPlaceholders()
        const {
        return addrPlaceholders;
    }

private:
    /// List of all functions in the module
    List<Function> funcs;

    /// Constant pool
    utl::node_hashmap<std::pair<uint64_t, size_t>, Constant> constants;

    /// Undef constant
    std::unique_ptr<UndefValue> undef;

    /// Data section
    std::vector<uint8_t> staticData;

    ///
    utl::small_vector<std::pair<size_t, Function const*>> addrPlaceholders;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_MODULE_H_
