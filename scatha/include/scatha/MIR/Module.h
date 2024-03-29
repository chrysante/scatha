#ifndef SCATHA_MIR_MODULE_H_
#define SCATHA_MIR_MODULE_H_

#include <span>
#include <string>
#include <vector>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/List.h>
#include <scatha/Common/Metadata.h>
#include <scatha/MIR/Fwd.h>

namespace scatha::mir {

/// Represents one unit of translation
class SCTEST_API Module: public ObjectWithMetadata {
    template <typename T>
    static decltype(auto) asDerived(auto&& self) {
        return static_cast<T&>(self);
    }

public:
    /// Lifetime functions @{
    Module();
    Module(Module const&) = delete;
    Module(Module&&) noexcept;
    Module& operator=(Module const&) = delete;
    Module& operator=(Module&&) noexcept;
    ~Module();
    /// @}

    /// Add a function to this translation unit
    void addFunction(Function* function);

    /// For now used for foreign functions
    void addGlobal(Value* value);

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

    /// View over all foreign functions in this module
    std::span<ForeignFunction* const> foreignFunctionsNEW() {
        return foreignFuncsNEW.values();
    }

    /// \overload
    std::span<ForeignFunction const* const> foreignFunctionsNEW() const {
        return foreignFuncsNEW.values();
    }

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

    utl::hashset<ForeignFunction*> foreignFuncsNEW;

    utl::hashset<UniquePtr<Value>> _globals;

    /// Data section
    std::vector<uint8_t> staticData;

    ///
    utl::small_vector<std::pair<size_t, Function const*>> addrPlaceholders;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_MODULE_H_
