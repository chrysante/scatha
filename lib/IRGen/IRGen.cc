#include "IRGen/IRGen.h"

#include <queue>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Common/DebugInfo.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/FunctionGeneration.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/Maps.h"
#include "IRGen/MetaData.h"
#include "Sema/AnalysisResult.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;

void irgen::generateIR(ir::Context& ctx,
                       ir::Module& mod,
                       ast::ASTNode const&,
                       sema::SymbolTable const& sym,
                       sema::AnalysisResult const& analysisResult,
                       Config config) {
    TypeMap typeMap(ctx);
    for (auto* semaType: analysisResult.structDependencyOrder) {
        generateType(semaType, ctx, mod, typeMap);
    }
    FunctionMap functionMap;
    auto queue = analysisResult.functions |
                 transform([](auto* def) { return def->function(); }) |
                 filter([](auto* fn) {
                     return fn->binaryVisibility() ==
                            sema::BinaryVisibility::Export;
                 }) |
                 ranges::to<std::deque<sema::Function const*>>;
    for (auto* semaFn: queue) {
        declareFunction(semaFn, ctx, mod, typeMap, functionMap);
    }
    while (!queue.empty()) {
        auto* semaFn = queue.front();
        queue.pop_front();
        auto* irFn = functionMap(semaFn);
        auto* native = dyncast<ir::Function*>(irFn);
        if (!native) continue;
        generateFunction(config,
                         { .semaFn = *semaFn,
                           .irFn = *native,
                           .ctx = ctx,
                           .mod = mod,
                           .symbolTable = sym,
                           .typeMap = typeMap,
                           .functionMap = functionMap,
                           .declQueue = queue });
    }
    ir::assertInvariants(ctx, mod);
    if (config.generateDebugSymbols) {
        mod.setMetadata(config.sourceFiles | transform(&SourceFile::path) |
                        ranges::to<dbi::SourceFileList>);
    }
}
