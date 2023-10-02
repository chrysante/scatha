#ifndef SCATHA_IRGEN_FUNCTIONGENERATION_H_
#define SCATHA_IRGEN_FUNCTIONGENERATION_H_

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Builder.h"
#include "IR/Fwd.h"
#include "IRGen/CallingConvention.h"
#include "IRGen/Maps.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Generates IR for the function \p semaFn
/// Dispatches to `generateAstFunction()` or `generateSynthFunction()`
utl::small_vector<sema::Function const*> generateFunction(
    sema::Function const& semaFn,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap);

/// Lowers the user defined function \p funcDecl from AST representation into
/// IR. The result is written into \p irFn
/// \Returns a list of functions called by the lowered function that are not yet
/// defined. This mechanism is part of lazy function generation.
utl::small_vector<sema::Function const*> generateAstFunction(
    ast::FunctionDefinition const& funcDecl,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap);

/// Generates IR for the compiler generated function \p semaFn
utl::small_vector<sema::Function const*> generateSynthFunction(
    sema::Function const& semaFn,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap);

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
    utl::vector<sema::Function const*>& declaredFunctions;

    FuncGenContextBase(sema::Function const& semaFn,
                       ir::Function& irFn,
                       ir::Context& ctx,
                       ir::Module& mod,
                       sema::SymbolTable const& symbolTable,
                       TypeMap const& typeMap,
                       FunctionMap& functionMap,
                       utl::vector<sema::Function const*>& declaredFunctions):
        FunctionBuilder(ctx, &irFn),
        semaFn(semaFn),
        irFn(irFn),
        ctx(ctx),
        mod(mod),
        symbolTable(symbolTable),
        typeMap(typeMap),
        functionMap(functionMap),
        declaredFunctions(declaredFunctions) {}

    /// Map \p semaFn to the corresponding IR function. If the function is not
    /// declared it will be declared.
    ir::Callable* getFunction(sema::Function const* semaFn);

    ///
    ir::ForeignFunction* getMemcpy();

    /// Emit a call to `memcpy`
    void callMemcpy(ir::Value* dest, ir::Value* source, ir::Value* numBytes);

    /// \overload for `size_t numBytes`
    void callMemcpy(ir::Value* dest, ir::Value* source, size_t numBytes);

    /// Get the calling convention of \p function
    CallingConvention const& getCC(sema::Function const* function);
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCTIONGENERATION_H_
