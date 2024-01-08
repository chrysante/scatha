#ifndef SCATHA_RUNTIME_COMPILER_H_
#define SCATHA_RUNTIME_COMPILER_H_

#include <filesystem>
#include <functional>
#include <string>
#include <typeindex>
#include <typeinfo>

#include <scatha/Common/SourceFile.h>
#include <scatha/Runtime/Library.h>
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
    /// Construct an empty compiler
    Compiler();

    /// For now because we rely on address stability of the symbol table
    Compiler(Compiler const&) = delete;

    /// Declares the type described by \p desc to the internal symbol table and
    /// returns a pointer to it
    sema::StructType const* declareType(StructDesc desc) {
        return lib.declareType(desc);
    }

    /// Declares the function described by \p desc to the internal symbol table
    FuncDecl declareFunction(std::string name, sema::FunctionType const* type) {
        return lib.declareFunction(std::move(name), type);
    }

    /// \overload
    template <ValidFunction F>
    FuncDecl declareFunction(std::string name, F&& f) {
        return lib.declareFunction<F>(std::move(name), std::forward<F>(f));
    }

    /// Maps the C++ type \p key to the Scatha type \p value
    sema::Type const* mapType(std::type_info const& key,
                              sema::Type const* value) {
        return lib.mapType(key, value);
    }

    /// Equivalent to `mapType(key, declareType(valueDesc))`
    sema::Type const* mapType(std::type_info const& key, StructDesc valueDesc) {
        return lib.mapType(key, std::move(valueDesc));
    }

    /// \Returns the Scatha type mapped to the C++ type \p key
    sema::Type const* getType(std::type_info const& key) const {
        return lib.getType(key);
    }

    /// \Returns the Scatha type mapped to the C++ type `T`
    template <typename T>
    sema::Type const* getType() const {
        return lib.getType<T>();
    }

    /// Adds source code from memory
    void addSourceText(std::string text, std::filesystem::path path = {});

    /// Loads source code from file
    void addSourceFile(std::filesystem::path path);

    ///
    Program compile();

    ///
    template <typename F>
    sema::FunctionType const* extractFunctionType() const {
        return lib.extractFunctionType<F>();
    }

private:
    sema::SymbolTable sym;
    Library lib;
    std::vector<SourceFile> sourceFiles;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_COMPILER_H_
