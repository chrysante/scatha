#include "AST/Lowering/LoweringContext.h"

#include "AST/AST.h"
#include "AST/LowerToIR.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace ast;

[[nodiscard]] std::pair<ir::Context, ir::Module> ast::lowerToIR(
    ASTNode const& root,
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
    mod(mod) {
    arrayViewType = ctx.anonymousStructure(
        std::array<ir::Type const*, 2>{ ctx.pointerType(),
                                        ctx.integralType(64) });
}

void LoweringContext::run(ast::ASTNode const& root) {
    makeDeclarations();
    generate(root);
}
