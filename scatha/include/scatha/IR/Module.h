#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_

#include <span>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/List.h>
#include <scatha/Common/Metadata.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Represents a program.
/// Contains functions, other globals and user defined types.
/// Acts like a range of functions
class SCATHA_API Module: public ObjectWithMetadata {
public:
    Module();
    Module(Module const& rhs) = delete;
    Module(Module&& rhs) noexcept;
    Module& operator=(Module const& rhs) = delete;
    Module& operator=(Module&& rhs) noexcept;
    ~Module();

    /// \Returns a view over the user defined structs in this module
    auto structures() const { return structs | Opaque; }

    /// \Returns a view over the functions in this module
    auto& functions() { return funcs; }

    /// \overload
    auto const& functions() const { return funcs; }

    /// \Returns a view over all foreign function declarations in this module
    std::span<ForeignFunction* const> extFunctions() {
        return _extFunctions.values();
    }

    /// \overload
    std::span<ForeignFunction const* const> extFunctions() const {
        return _extFunctions.values();
    }

    /// \Returns a view over all globals in this module, i.e. global variables
    /// and foreign functions
    auto& globals() { return _globals; }

    /// \overload
    auto const& globals() const { return _globals; }

    /// Add a structure type to this module
    StructType const* addStructure(UniquePtr<StructType> structure);

    /// Add a global to this module
    Global* addGlobal(UniquePtr<Global> value);

    /// \overload that down casts to the given type
    template <std::derived_from<Global> G>
    G* addGlobal(UniquePtr<G> value) {
        return cast<G*>(addGlobal(uniquePtrCast<Global>(std::move(value))));
    }

    /// Creates a global constant with value \p value and name \p name if no
    /// global constant with the same value exists yet. Otherwise returns the
    /// existing constant. This function is used to allocate global constants to
    /// unique them.
    GlobalVariable* makeGlobalConstant(Context& ctx, Constant* value,
                                       std::string name);

    /// Erase the global  \p global from this module.  \p global can also be a
    /// function
    void erase(Global* global);

    /// \overload
    List<Function>::iterator erase(List<Function>::const_iterator funcItr);

    /// Functio container interface @{
    List<Function>::iterator begin();
    List<Function>::const_iterator begin() const;
    List<Function>::iterator end();
    List<Function>::const_iterator end() const;
    bool empty() const;
    Function& front();
    Function const& front() const;
    Function& back();
    Function const& back() const;
    /// @}

private:
    utl::vector<UniquePtr<StructType>> structs;
    List<Global> _globals;
    utl::hashset<ForeignFunction*> _extFunctions;
    /// Map used to unique global constants
    utl::hashmap<Constant*, GlobalVariable*> globalConstMap;
    List<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_
