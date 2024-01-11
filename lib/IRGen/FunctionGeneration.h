#ifndef SCATHA_IRGEN_FUNCTIONGENERATION_H_
#define SCATHA_IRGEN_FUNCTIONGENERATION_H_

#include <deque>

#include <svm/Builtin.h>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Builder.h"
#include "IR/Fwd.h"
#include "IRGen/CallingConvention.h"
#include "IRGen/IRGen.h"
#include "IRGen/Maps.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

///
struct FuncGenParameters {
    sema::Function const& semaFn;
    ir::Function& irFn;
    ir::Context& ctx;
    ir::Module& mod;
    sema::SymbolTable const& symbolTable;
    TypeMap const& typeMap;
    FunctionMap& functionMap;
    std::deque<sema::Function const*>& declQueue;
};

/// Generates IR for the function \p semaFn
/// Dispatches to `generateAstFunction()` or `generateSynthFunction()`
void generateFunction(Config config, FuncGenParameters);

/// Lowers the user defined function \p funcDecl from AST representation into
/// IR. The result is written into \p irFn
/// \Returns a list of functions called by the lowered function that are not yet
/// defined. This mechanism is part of lazy function generation.
void generateAstFunction(Config config, FuncGenParameters);

/// Generates IR for the compiler generated function \p semaFn
void generateSynthFunction(Config config, FuncGenParameters);

/// Generates IR for the function \p semaFn as if it were a special lifetime
/// function of kind \p kind
/// This is used to generate default construction and destruction of member
/// objects in user defined lifetime functions
void generateSynthFunctionAs(sema::SpecialLifetimeFunction kind,
                             Config config,
                             FuncGenParameters);

/// Base class of context objects for function generation of both user defined
/// and compiler generated functions
struct FuncGenContextBase: FuncGenParameters, ir::FunctionBuilder {
    Config config;
    ValueMap valueMap;
    ir::StructType const* arrayPtrType = nullptr;

    using FuncGenParameters::ctx;

    FuncGenContextBase(Config config, FuncGenParameters params);

    /// Map \p semaFn to the corresponding IR function. If the function is not
    /// declared it will be declared.
    ir::Callable* getFunction(sema::Function const* semaFn);

    /// Get the calling convention of \p function
    CallingConvention const& getCC(sema::Function const* function);

    ///
    ir::ForeignFunction* getBuiltin(svm::Builtin builtin);

    /// Emit a call to `memcpy`
    ir::Call* callMemcpy(ir::Value* dest,
                         ir::Value* source,
                         ir::Value* numBytes);

    /// \overload for `size_t numBytes`
    ir::Call* callMemcpy(ir::Value* dest, ir::Value* source, size_t numBytes);

    /// Emit a call to `memset`
    ir::Call* callMemset(ir::Value* dest, ir::Value* numBytes, int value);

    /// \overload for `size_t numBytes`
    ir::Call* callMemset(ir::Value* dest, size_t numBytes, int value);

    /// Converts array pointer (fat pointers) to normal pointers by extracting
    /// the first element. All other arguments are returned unchanged.
    ir::Value* toThinPointer(ir::Value* ptr);

    ///
    ir::Value* makeArrayPointer(ir::Value* addr, ir::Value* count);
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCTIONGENERATION_H_
