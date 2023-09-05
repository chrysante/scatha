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

UniquePtr<ast::ConstructorCall> sema::makeConstructorCall(
    sema::ObjectType const* type,
    utl::small_vector<UniquePtr<ast::Expression>> arguments,
    DTorStack& dtors,
    SymbolTable& sym,
    IssueHandler& iss,
    SourceRange sourceRange) {
    auto* structType = dyncast<StructureType const*>(type);
    if (!structType) {
        return nullptr;
    }
    using enum SpecialMemberFunction;
    auto* ctorSet = structType->specialMemberFunction(New);
    if (!ctorSet) {
        return nullptr;
    }
    utl::small_vector<QualType> argTypes = { sym.explRef(type) };
    argTypes.reserve(1 + arguments.size());
    ranges::transform(arguments, std::back_inserter(argTypes), [](auto& expr) {
        return expr->type();
    });
    auto result = performOverloadResolution(ctorSet, argTypes, true);
    if (result.error) {
        result.error->setSourceRange(sourceRange);
        iss.push(std::move(result.error));
        return nullptr;
    }
    auto ctorCall = allocate<ast::ConstructorCall>(std::move(arguments),
                                                   sourceRange,
                                                   result.function,
                                                   SpecialMemberFunction::New);
    ctorCall->decorate(&sym.addTemporary(structType), structType);
    convertArguments(*ctorCall, result, dtors, sym, iss);
    return ctorCall;
}
