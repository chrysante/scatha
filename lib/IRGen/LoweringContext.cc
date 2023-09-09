#include "IRGen/LoweringContext.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/IRGen.h"

using namespace scatha;
using namespace irgen;

[[nodiscard]] std::pair<ir::Context, ir::Module> irgen::generateIR(
    ast::ASTNode const& root,
    sema::SymbolTable const& symbolTable,
    sema::AnalysisResult const& analysisResult) {
    ir::Context ctx;
    ir::Module mod;
    LoweringContext context(symbolTable, analysisResult, ctx, mod);
    context.run(root);
    ir::setupInvariants(ctx, mod);
    ir::assertInvariants(ctx, mod);
    return { std::move(ctx), std::move(mod) };
}

LoweringContext::LoweringContext(sema::SymbolTable const& symbolTable,
                                 sema::AnalysisResult const& analysisResult,
                                 ir::Context& ctx,
                                 ir::Module& mod):
    symbolTable(symbolTable),
    analysisResult(analysisResult),
    ctx(ctx),
    mod(mod),
    typeMap(ctx) {
    arrayViewType = ctx.anonymousStruct(
        std::array<ir::Type const*, 2>{ ctx.ptrType(), ctx.intType(64) });
}

void LoweringContext::run(ast::ASTNode const& root) {
    makeDeclarations();
    generate(root);
}
