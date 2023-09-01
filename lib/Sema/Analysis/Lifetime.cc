#include "Sema/Analysis/Lifetime.h"

#include <array>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

UniquePtr<ast::ConstructorCall> sema::makeConstructorCall(
    sema::ObjectType const* type,
    std::span<UniquePtr<ast::Expression>> arguments,
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
    auto function = performOverloadResolution(ctorSet, argTypes, true);
    if (!function) {
        function.error()->setSourceRange(sourceRange);
        iss.push(function.error());
        return nullptr;
    }
    auto result = allocate<ast::ConstructorCall>(arguments,
                                                 function.value(),
                                                 SpecialMemberFunction::New);
    result->decorate(&sym.addTemporary(structType), structType);
    return result;
}