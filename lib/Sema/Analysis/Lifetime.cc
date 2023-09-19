#include "Sema/Analysis/Lifetime.h"

#include <array>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;

UniquePtr<ast::ConstructorCall> sema::makeConstructorCall(
    sema::ObjectType const* type,
    UniquePtr<ast::Expression> objectArgument,
    utl::small_vector<UniquePtr<ast::Expression>> arguments,
    DTorStack& dtors,
    Context& ctx,
    SourceRange sourceRange) {
    auto& sym = ctx.symbolTable();
    auto* structType = dyncast<StructType const*>(type);
    if (!structType) {
        return nullptr;
    }
    if (!objectArgument) {
        objectArgument = allocate<ast::UninitTemporary>(sourceRange);
        objectArgument->decorateValue(sym.temporary(type), LValue);
    }
    arguments.insert(arguments.begin(), std::move(objectArgument));
    using enum SpecialMemberFunction;
    auto* ctorSet = structType->specialMemberFunction(New);
    if (!ctorSet) {
        return nullptr;
    }
    auto result =
        performOverloadResolution(ctorSet,
                                  arguments | ToAddress | ToSmallVector<>,
                                  /* isMemberCall = */ true);
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
