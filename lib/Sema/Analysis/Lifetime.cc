#include "Sema/Analysis/Lifetime.h"

#include <array>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

static bool canConstructTrivialType(
    ObjectType const* type,
    utl::small_vector<UniquePtr<ast::Expression>>& arguments,
    DTorStack& dtors,
    AnalysisContext& ctx) {
    if (arguments.size() == 0) {
        return true;
    }
    if (arguments.size() == 1 &&
        (!isa<StructType>(type) || type == arguments.front()->type().get()))
    {
        /// We convert explicitly here because it allows expressions like
        /// `int(1.0)`
        auto* arg =
            convert(Explicit,
                    arguments.front().get(),
                    getQualType(type, Mutability::Const),
                    LValue, // So we don't end up in infinite recursion :/
                    dtors,
                    ctx);
        if (arg) {
            arguments.front().release();
            arguments.front() = UniquePtr<ast::Expression>(arg);
            return true;
        }
    }
    if (auto* structType = dyncast<StructType const*>(type)) {
        if (arguments.size() != structType->members().size()) {
            return false;
        }
        bool success = true;
        for (auto&& [arg, type]:
             ranges::views::zip(arguments, structType->members()))
        {
            auto* converted = convert(Implicit,
                                      arg.get(),
                                      getQualType(type, Mutability::Const),
                                      RValue,
                                      dtors,
                                      ctx);
            if (converted) {
                arg.release();
                arg = UniquePtr<ast::Expression>(converted);
            }
            else {
                success = false;
            }
        }
        return success;
    }
    return false;
}

UniquePtr<ast::Expression> sema::makePseudoConstructorCall(
    sema::ObjectType const* type,
    UniquePtr<ast::Expression> objectArgument,
    utl::small_vector<UniquePtr<ast::Expression>> arguments,
    DTorStack& dtors,
    AnalysisContext& ctx,
    SourceRange sourceRange) {
    using enum SpecialMemberFunction;

    auto& sym = ctx.symbolTable();
    auto& iss = ctx.issueHandler();
    auto* structType = dyncast<StructType const*>(type);
    if (!structType || !structType->specialMemberFunction(New)) {
        if (canConstructTrivialType(type, arguments, dtors, ctx)) {
            auto expr =
                allocate<ast::TrivialConstructExpr>(std::move(arguments),
                                                    type,
                                                    sourceRange);
            expr->decorateValue(sym.temporary(type), RValue);
            return expr;
        }
        /// TODO: Push an error here!
        SC_UNIMPLEMENTED();
    }
    if (!objectArgument) {
        objectArgument = allocate<ast::UninitTemporary>(sourceRange);
        objectArgument->decorateValue(sym.temporary(type), LValue);
    }
    arguments.insert(arguments.begin(), std::move(objectArgument));
    auto* ctorSet = structType->specialMemberFunction(New);
    SC_ASSERT(ctorSet, "Trivial lifetime case is handled above");
    auto result =
        performOverloadResolution(ctorSet,
                                  arguments | ToAddress | ToSmallVector<>,
                                  ORKind::MemberFunction);
    if (result.error) {
        result.error->setSourceRange(sourceRange);
        ctx.issueHandler().push(std::move(result.error));
        return nullptr;
    }
    auto ctorCall = allocate<ast::ConstructorCall>(std::move(arguments),
                                                   sourceRange,
                                                   result.function,
                                                   SpecialMemberFunction::New);
    ctorCall->decorateValue(sym.temporary(structType), RValue);
    convertArguments(*ctorCall, result, dtors, ctx);
    return ctorCall;
}
