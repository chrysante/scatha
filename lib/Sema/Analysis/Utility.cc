#include "Sema/Analysis/Utility.h"

#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Context.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

ast::Statement* sema::parentStatement(ast::ASTNode* node) {
    while (true) {
        if (!node) {
            return nullptr;
        }
        if (auto* stmt = dyncast<ast::Statement*>(node)) {
            return stmt;
        }
        node = node->parent();
    }
}

void sema::convertArguments(ast::CallLike& fc,
                            OverloadResolutionResult const& orResult,
                            DTorStack& dtors,
                            Context& ctx) {
    for (auto [index, _arg, conv]: ranges::views::zip(ranges::views::iota(0),
                                                      fc.arguments(),
                                                      orResult.conversions))
    {
        auto* arg = _arg;
        if (!conv.isNoop()) {
            arg = insertConversion(arg, conv);
        }
        /// If our argument is an lvalue of struct type  we need to call the
        /// copy constructor if there is one
        if (!arg->isLValue()) {
            continue;
        }
        copyValue(arg, dtors, ctx);
    }
}

ast::Expression* sema::copyValue(ast::Expression* expr,
                                 DTorStack& dtors,
                                 Context& ctx) {
    auto& sym = ctx.symbolTable();
    auto structType = nonTrivialLifetimeType(expr->type().get());
    if (!structType) {
        return expr;
    }
    using enum SpecialLifetimeFunction;
    auto* copyCtor = structType->specialLifetimeFunction(CopyConstructor);
    SC_ASSERT(copyCtor, "Must exists because we are non-trivial lifetime");
    expr = convertExplicitly(expr,
                             sym.explRef(QualType::Const(structType)),
                             dtors,
                             ctx);
    auto sourceRange = expr->sourceRange();
    size_t index = expr->indexInParent();
    auto* parent = expr->parent();
    auto ctorCall =
        allocate<ast::ConstructorCall>(toSmallVector(expr->extractFromParent()),
                                       sourceRange,
                                       copyCtor,
                                       SpecialMemberFunction::New);
    ctorCall->decorate(&sym.addTemporary(structType), structType);
    dtors.push(ctorCall->object());
    auto* result = ctorCall.get();
    parent->setChild(index, std::move(ctorCall));
    return result;
}

StructureType const* sema::nonTrivialLifetimeType(ObjectType const* type) {
    auto structType = dyncast<StructureType const*>(type);
    if (!structType || structType->hasTrivialLifetime()) {
        return nullptr;
    }
    return structType;
}
