#ifndef SCATHA_IRGEN_FUNCTIONGENERATION_H_
#define SCATHA_IRGEN_FUNCTIONGENERATION_H_

#include <deque>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/IRGen.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class TypeMap;
class FunctionMap;

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
void generateNativeFunction(Config config, FuncGenParameters);

/// Generates IR for the compiler generated function \p semaFn
void generateSynthFunction(Config config, FuncGenParameters);

/// Generates IR for the function \p semaFn as if it were a special lifetime
/// function of kind \p kind
/// This is used to generate default construction and destruction of member
/// objects in user defined lifetime functions
void generateSynthFunctionAs(sema::SMFKind kind, Config config,
                             FuncGenParameters);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCTIONGENERATION_H_
