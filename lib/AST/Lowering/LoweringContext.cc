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

[[nodiscard]] SCATHA_API ir::Module ast::lowerToIR(
    AbstractSyntaxTree const& root,
    sema::SymbolTable const& symbolTable,
    ir::Context& ctx) {
    ir::Module mod;
    LoweringContext context(symbolTable, ctx, mod);
    context.run(root);
    ir::setupInvariants(ctx, mod);
    ir::assertInvariants(ctx, mod);
    return mod;
}

LoweringContext::LoweringContext(sema::SymbolTable const& symbolTable,
                                 ir::Context& ctx,
                                 ir::Module& mod):
    symbolTable(symbolTable), ctx(ctx), mod(mod) {
    arrayViewType = ctx.anonymousStructure(
        std::array<ir::Type const*, 2>{ ctx.pointerType(),
                                        ctx.integralType(64) });
}

void LoweringContext::run(ast::AbstractSyntaxTree const& root) {
    makeDeclarations();
    generate(root);
}
