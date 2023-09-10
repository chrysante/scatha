#include "IRGen/IRGen.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IRGen/GenerateFunction.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/CFG.h"
#include "IR/Validate.h"
#include "IR/Validate.h"
#include "IRGen/Globals.h"
#include "IRGen/Maps.h"
#include "IRGen/MetaData.h"
#include "Sema/AnalysisResult.h"
#include "Sema/Entity.h"


using namespace scatha;
using namespace irgen;

std::pair<ir::Context, ir::Module> irgen::generateIR2(
    ast::ASTNode const& root,
    sema::SymbolTable const& sym,
    sema::AnalysisResult const& analysisResult) {
    ir::Context ctx;
    ir::Module mod;
    TypeMap typeMap(ctx);

    for (auto* semaType: analysisResult.structDependencyOrder) {
        auto [irType, metaData] = generateType(ctx, typeMap, semaType);
        typeMap.insert(semaType, irType.get(), std::move(metaData));
        mod.addStructure(std::move(irType));
    }

    FunctionMap functionMap;
    
    for (auto* funcDecl: analysisResult.functions) {
        auto* semaFn = funcDecl->function();
        auto [irFnOwner, metaData] = declareFunction(ctx, typeMap, semaFn);
        auto* irFn = irFnOwner.get();
        functionMap.insert(semaFn, irFn, std::move(metaData));
        mod.addGlobal(std::move(irFnOwner));
         generateFunction(*funcDecl,
                              cast<ir::Function&>(*irFn),
                              ctx,
                              mod,
                              sym,
                              typeMap,
                              functionMap);
    }
    
    
    ir::assertInvariants(ctx, mod);
    return { std::move(ctx), std::move(mod) };
}
