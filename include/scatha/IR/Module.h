#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_

#include <span>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/List.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>
#include <svm/Builtin.h>

namespace scatha::ir {

class SCATHA_API Module {
public:
    Module() = default;
    Module(Module const& rhs) = delete;
    Module(Module&& rhs) noexcept;
    Module& operator=(Module const& rhs) = delete;
    Module& operator=(Module&& rhs) noexcept;
    ~Module();

    ///
    auto structures() { return structs | Opaque; }

    ///
    auto structures() const { return structs | Opaque; }

    /// View over the globals in this module
    auto globals() const { return _globals | ToConstAddress; }

    ///
    auto& functions() { return funcs; }

    ///
    auto const& functions() const { return funcs; }

    /// \Returns The `ForeignFunction` in slot \p slot at index \p index
    ForeignFunction* extFunction(size_t slot, size_t index);

    /// \Returns The `ForeignFunction` in slot `svm::BuiltinFunctionSlot` at
    /// index \p builtin
    ForeignFunction* builtinFunction(svm::Builtin builtin);

    /// Add a structure type to this module
    StructType const* addStructure(UniquePtr<StructType> structure);

    /// Add a global value to this module
    Global* addGlobal(UniquePtr<Global> value);

    /// \overload that down casts to the given type
    template <std::derived_from<Global> G>
    G* addGlobal(UniquePtr<G> value) {
        return cast<G*>(addGlobal(uniquePtrCast<Global>(std::move(value))));
    }

    ///
    void eraseFunction(Function* function);

    ///
    List<Function>::iterator eraseFunction(List<Function>::const_iterator itr);

    ///
    List<Function>::iterator begin();
    List<Function>::const_iterator begin() const;
    List<Function>::iterator end();
    List<Function>::const_iterator end() const;
    bool empty() const;
    Function& front();
    Function const& front() const;
    Function& back();
    Function const& back() const;

private:
    utl::vector<UniquePtr<StructType>> structs;
    utl::vector<UniquePtr<Global>> _globals;
    utl::hashmap<std::pair<size_t, size_t>, ForeignFunction*> _extFunctions;
    List<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_
