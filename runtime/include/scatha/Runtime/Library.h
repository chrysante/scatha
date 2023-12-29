#ifndef SCATHA_RUNTIME_LIBRARY_H_
#define SCATHA_RUNTIME_LIBRARY_H_

#include <string>

#include <utl/vector.hpp>

#include <scatha/Runtime/Support.h>
#include <scatha/Sema/Entity.h>
#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

class SymbolTable;

} // namespace scatha::sema

namespace scatha {

///
class Library {
public:
    ///
    explicit Library(sema::SymbolTable& sym, size_t slot);

    /// Declares the type described by \p desc to the internal symbol table and
    /// returns a pointer to it
    sema::StructType const* declareType(StructDesc desc);

    /// Declares the function described by \p desc to the internal symbol table
    FuncDecl declareFunction(std::string name,
                             sema::FunctionSignature signature);

    /// \overload
    template <ValidFunction F>
    FuncDecl declareFunction(std::string name, F&& f);

    /// Maps the C++ type \p key to the Scatha type \p value
    sema::Type const* mapType(std::type_info const& key,
                              sema::Type const* value);

    /// Equivalent to `mapType(key, declareType(valueDesc))`
    sema::Type const* mapType(std::type_info const& key, StructDesc valueDesc);

    /// \Returns the Scatha type mapped to the C++ type \p key
    sema::Type const* getType(std::type_info const& key) const;

    /// \Returns the Scatha type mapped to the C++ type `T`
    template <typename T>
    sema::Type const* getType() const {
        return getType(typeid(T));
    }

    ///
    template <typename F>
    sema::FunctionSignature extractSignature() const;

private:
    sema::SymbolTable* sym = nullptr;
    std::unordered_map<std::type_index, sema::Type const*> typemap;
    size_t _slot = 0;
    size_t _index = 0;
};

} // namespace scatha

// ========================================================================== //
// ===  Inline implementation  ============================================== //
// ========================================================================== //

template <scatha::ValidFunction F>
scatha::FuncDecl scatha::Library::declareFunction(std::string name, F&&) {
    return declareFunction(std::move(name), extractSignature<F>());
}

namespace scatha::internal {

template <typename>
struct ExtractSig;

template <typename R, typename... Args>
struct ExtractSig<std::function<R(Args...)>> {
    static sema::FunctionSignature Impl(Library const* self) {
        utl::small_vector<sema::Type const*> argTypes;
        argTypes.reserve(sizeof...(Args));
        (argTypes.push_back(self->getType<Args>()), ...);
        auto* retType = self->getType<R>();
        return sema::FunctionSignature(std::move(argTypes), retType);
    };
};

} // namespace scatha::internal

template <typename F>
scatha::sema::FunctionSignature scatha::Library::extractSignature() const {
    return internal::ExtractSig<decltype(std::function{
        std::declval<F>() })>::Impl(this);
}

#endif // SCATHA_RUNTIME_LIBRARY_H_
