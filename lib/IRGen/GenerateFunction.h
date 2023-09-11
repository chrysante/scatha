#ifndef SCATHA_IRGEN_GENERATEFUNCTION_H_
#define SCATHA_IRGEN_GENERATEFUNCTION_H_

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class TypeMap;
class FunctionMap;

///
utl::small_vector<sema::Function const*> generateFunction(
    ast::FunctionDefinition const& funcDecl,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_GENERATEFUNCTION_H_