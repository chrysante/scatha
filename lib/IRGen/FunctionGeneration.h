#ifndef SCATHA_IRGEN_FUNCTIONGENERATION_H_
#define SCATHA_IRGEN_FUNCTIONGENERATION_H_

#include <deque>

#include <svm/Builtin.h>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Builder.h"
#include "IR/Fwd.h"
#include "IRGen/CallingConvention.h"
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
void generateFunction(FuncGenParameters);

/// Lowers the user defined function \p funcDecl from AST representation into
/// IR. The result is written into \p irFn
/// \Returns a list of functions called by the lowered function that are not yet
/// defined. This mechanism is part of lazy function generation.
void generateAstFunction(FuncGenParameters);

/// Generates IR for the compiler generated function \p semaFn
void generateSynthFunction(FuncGenParameters);

/// Base class of context objects for function generation of both user defined
/// and compiler generated functions
struct FuncGenContextBase: ir::FunctionBuilder {
    /// Global references
    sema::Function const& semaFn;
    ir::Function& irFn;
    ir::Context& ctx;
    ir::Module& mod;
    sema::SymbolTable const& symbolTable;
    TypeMap const& typeMap;
    FunctionMap& functionMap;
    std::deque<sema::Function const*>& declQueue;

    FuncGenContextBase(FuncGenParameters params):
        FunctionBuilder(params.ctx, &params.irFn),
        semaFn(params.semaFn),
        irFn(params.irFn),
        ctx(params.ctx),
        mod(params.mod),
        symbolTable(params.symbolTable),
        typeMap(params.typeMap),
        functionMap(params.functionMap),
        declQueue(params.declQueue) {}

    /// Map \p semaFn to the corresponding IR function. If the function is not
    /// declared it will be declared.
    ir::Callable* getFunction(sema::Function const* semaFn);

    ///
    ir::ForeignFunction* getBuiltin(svm::Builtin builtin);

    /// Emit a call to `memcpy`
    void callMemcpy(ir::Value* dest, ir::Value* source, ir::Value* numBytes);

    /// \overload for `size_t numBytes`
    void callMemcpy(ir::Value* dest, ir::Value* source, size_t numBytes);

    /// Emit a call to `memset`
    void callMemset(ir::Value* dest, ir::Value* numBytes, int value);

    /// \overload for `size_t numBytes`
    void callMemset(ir::Value* dest, size_t numBytes, int value);

    /// Get the calling convention of \p function
    CallingConvention const& getCC(sema::Function const* function);
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCTIONGENERATION_H_
