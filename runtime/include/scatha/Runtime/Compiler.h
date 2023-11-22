#ifndef SCATHA_RUNTIME_COMPILER_H_
#define SCATHA_RUNTIME_COMPILER_H_

#include <filesystem>
#include <string>
#include <typeindex>
#include <typeinfo>

#include <scatha/Common/SourceFile.h>
#include <scatha/Runtime/Support.h>
#include <scatha/Sema/Entity.h>
#include <scatha/Sema/SymbolTable.h>

/// # Design Idea
///
/// Three major classes:
/// ## Compiler
/// Responsible for compilation pipeline. Can declare types and functions,
/// collect source files and perform the compilation. Function declaration
/// returns an object that bundles slot, index, name and signature
///
/// ## Executor
/// Wraps the VM. Supplies implementations for declared functions.
///
/// ## Runtime
/// Bundles `Compiler` and `Executor` for ease of use. Functions can be declared
/// and defined in a single step.

namespace scatha {

class Program;
class Runtime;

///
class Compiler {
public:
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

    /// Adds source code from memory
    void addSourceText(std::string text, std::filesystem::path path = {});

    /// Loads source code from file
    void addSourceFile(std::filesystem::path path);

    ///
    Program compile();

private:
    friend class Runtime; // For extractSignature()

    template <typename F>
    sema::FunctionSignature extractSignature();

    sema::SymbolTable sym;
    std::unordered_map<std::type_index, sema::Type const*> typeMap;

    size_t functionIndex = 0;

    std::vector<SourceFile> sourceFiles;
};

} // namespace scatha

// ========================================================================== //
// ===  Inline implementation  ============================================== //
// ========================================================================== //

template <scatha::ValidFunction F>
scatha::FuncDecl scatha::Compiler::declareFunction(std::string name, F&&) {
    return declareFunction(std::move(name), extractSignature<F>());
}

template <typename F>
scatha::sema::FunctionSignature scatha::Compiler::extractSignature() {
    utl::small_vector<sema::Type const*> argTypes;
    argTypes.reserve(FunctionTraits<F>::ArgumentCount);
    [&]<size_t... I>(std::index_sequence<I...>) {
        (
            [&] {
            using Arg = typename FunctionTraits<F>::template ArgumentAt<I>;
            argTypes.push_back(getType<Arg>());
            }(),
            ...);
        }(std::make_index_sequence<FunctionTraits<F>::ArgumentCount>{});
    auto* retType = mapType<typename FunctionTraits<F>::ReturnType>();
    return sema::FunctionSignature(std::move(argTypes), retType);
}

#endif // SCATHA_RUNTIME_COMPILER_H_
