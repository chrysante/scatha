#include "IRGen/IRGen.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/GenerateFunction.h"
#include "IRGen/Globals.h"
#include "IRGen/Maps.h"
#include "IRGen/MetaData.h"
#include "Sema/AnalysisResult.h"
#include "Sema/Entity.h"

#include <utl/vector.hpp>

using namespace scatha;
using namespace irgen;

std::pair<ir::Context, ir::Module> irgen::generateIR(
    ast::ASTNode const& root,
    sema::SymbolTable const& sym,
    sema::AnalysisResult const& analysisResult) {
    ir::Context ctx;
    ir::Module mod;
    TypeMap typeMap(ctx);

    for (auto* semaType: analysisResult.structDependencyOrder) {
        generateType(semaType, ctx, mod, typeMap);
    }

    FunctionMap functionMap;

    utl::vector<ir::Function*> irFns;
    for (auto* funcDecl: analysisResult.functions) {
        auto* semaFn = funcDecl->function();
        auto* irFn = declareFunction(semaFn, ctx, mod, typeMap, functionMap);
        irFns.push_back(cast<ir::Function*>(irFn));
    }
    for (auto [funcDecl, irFn]:
         ranges::views::zip(analysisResult.functions, irFns))
    {
        generateFunction(*funcDecl, *irFn, ctx, mod, sym, typeMap, functionMap);
    }

    ir::assertInvariants(ctx, mod);
    return { std::move(ctx), std::move(mod) };
}
