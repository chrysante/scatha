#ifndef SCATHA_IRGEN_LOWERINGCONTEXT_H_
#define SCATHA_IRGEN_LOWERINGCONTEXT_H_

#include <deque>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/IRGen.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class TypeMap;
class GlobalMap;
struct GlobalVarMetadata;

/// Objects used during the entire lowering process
struct LoweringContext {
    ir::Context& ctx;
    ir::Module& mod;
    sema::SymbolTable const& symbolTable;
    TypeMap& typeMap;
    GlobalMap& globalMap;
    std::deque<sema::Function const*>& declQueue;
    Config const& config;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_LOWERINGCONTEXT_H_
