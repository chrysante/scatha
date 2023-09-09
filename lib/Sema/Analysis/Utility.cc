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

static void convertArgsImpl(auto&& args,
                            auto&& conversions,
                            DTorStack& dtors,
                            Context& ctx) {
    for (auto [arg, conv]: ranges::views::zip(args, conversions)) {
        auto* converted = insertConversion(arg, conv);
        /// If our argument is an lvalue of struct type  we need to call the
        /// copy constructor if there is one
        bool needCtorCall =
            conv.objectConversion() == ObjectTypeConversion::None &&
            converted->isRValue() && arg->isLValue();
        if (needCtorCall) {
            copyValue(converted, dtors, ctx);
        }
    }
}

void sema::convertArguments(ast::FunctionCall& fc,
                            OverloadResolutionResult const& orResult,
                            DTorStack& dtors,
                            Context& ctx) {
    convertArgsImpl(fc.arguments(), orResult.conversions, dtors, ctx);
}

void sema::convertArguments(ast::ConstructorCall& cc,
                            OverloadResolutionResult const& orResult,
                            DTorStack& dtors,
                            Context& ctx) {
    convertArgsImpl(cc.arguments(),
                    orResult.conversions | ranges::views::drop(1),
                    dtors,
                    ctx);
}

ast::Expression* sema::copyValue(ast::Expression* expr,
                                 DTorStack& dtors,
                                 Context& ctx) {
    SC_ASSERT(!isa<ReferenceType>(*expr->type()), "References are not values");
    auto& sym = ctx.symbolTable();
    auto* parent = expr->parent();
    size_t index = expr->indexInParent();
    auto type = expr->type();
    auto structType = nonTrivialLifetimeType(type.get());
    if (!structType) {
        auto copy = allocate<ast::TrivialCopyExpr>(expr->extractFromParent());
        copy->decorateExpr(nullptr, type);
        auto* result = copy.get();
        parent->setChild(index, std::move(copy));
        return result;
    }
    using enum SpecialLifetimeFunction;
    auto* copyCtor = structType->specialLifetimeFunction(CopyConstructor);
    SC_ASSERT(copyCtor, "Must exists because we are non-trivial lifetime");
    expr = convertExplicitly(expr,
                             sym.reference(QualType::Const(structType)),
                             dtors,
                             ctx);
    auto sourceRange = expr->sourceRange();
    auto ctorCall =
        allocate<ast::ConstructorCall>(toSmallVector(expr->extractFromParent()),
                                       sourceRange,
                                       copyCtor,
                                       SpecialMemberFunction::New);
    ctorCall->decorateExpr(&sym.addTemporary(structType));
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
