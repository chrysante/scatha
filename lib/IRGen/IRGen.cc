#include "IRGen/IRGen.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/Globals.h"
#include "IRGen/Maps.h"
#include "IRGen/MetaData.h"
#include "Sema/AnalysisResult.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;

std::pair<ir::Context, ir::Module> /*irgen::generateIR*/ TBA(
    ast::ASTNode const& root,
    sema::SymbolTable const& sym,
    sema::AnalysisResult const& analysisResult) {
    ir::Context ctx;
    ir::Module mod;
    TypeMap typeMap(ctx);

    for (auto* semaType: analysisResult.structDependencyOrder) {
        auto [irType, metaData] = generateType(ctx, typeMap, semaType);
        mod.addStructure(std::move(irType));
    }

    for (auto* semaFn: sym.functions()) {
        if (semaFn->isNative()) {
            auto [irFn, metaData] = declareFunction(ctx, typeMap, semaFn);
            mod.addGlobal(std::move(irFn));
        }
    }

    //    LoweringContext context(symbolTable, analysisResult, ctx, mod);
    //    context.run(root);

    ir::setupInvariants(ctx, mod);
    ir::assertInvariants(ctx, mod);
    return { std::move(ctx), std::move(mod) };
}
