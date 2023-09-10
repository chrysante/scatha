#include "IRGen/IRGen.h"

#include <queue>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

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
    auto queue =
        analysisResult.functions |
        ranges::views::transform([](auto* def) { return def->function(); }) |
        ranges::views::filter([](auto* fn) {
            return fn->accessSpecifier() == sema::AccessSpecifier::Public;
        }) |
        ranges::to<std::deque<sema::Function const*>>;
    for (auto* semaFn: queue) {
        declareFunction(semaFn, ctx, mod, typeMap, functionMap);
    }
    while (!queue.empty()) {
        auto* semaFn = queue.front();
        queue.pop_front();
        auto* irFn = functionMap(semaFn);
        auto decls = generateFunction(*semaFn->definition(),
                                      cast<ir::Function&>(*irFn),
                                      ctx,
                                      mod,
                                      sym,
                                      typeMap,
                                      functionMap);
        ranges::copy(decls, std::back_inserter(queue));
    }
    ir::assertInvariants(ctx, mod);
    return { std::move(ctx), std::move(mod) };
}
