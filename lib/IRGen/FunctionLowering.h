#ifndef SCATHA_IRGEN_FUNCTIONLOWERING_H_
#define SCATHA_IRGEN_FUNCTIONLOWERING_H_

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class TypeMap;
class FunctionMap;

/// Lowers the user defined function \p funcDecl from AST representation into
/// IR. The result is written into \p irFn
/// \Returns a list of functions called by the lowered function that are not yet
/// defined. This mechanism is part of lazy function generation.
utl::small_vector<sema::Function const*> lowerFunction(
    ast::FunctionDefinition const& funcDecl,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCTIONLOWERING_H_
