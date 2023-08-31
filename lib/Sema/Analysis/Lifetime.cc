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

UniquePtr<ast::LifetimeCall> sema::makeConstructorCall(
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
    utl::small_vector<QualType const*> argTypes = { sym.qualify(type,
                                                                RefMutExpl) };
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
    auto result = allocate<ast::LifetimeCall>(arguments,
                                              function.value(),
                                              SpecialMemberFunction::New);
    result->decorate(nullptr, sym.qualify(structType));
    return result;
}

UniquePtr<ast::LifetimeCall> sema::makeDestructorCall(sema::Object* object) {
    auto* type = object->type()->base();
    auto* structType = dyncast<StructureType const*>(type);
    if (!structType) {
        return nullptr;
    }
    using enum SpecialMemberFunction;
    auto* dtorSet = structType->specialMemberFunction(Delete);
    if (!dtorSet) {
        return nullptr;
    }
    SC_ASSERT(dtorSet->size() == 1, "Destructors cannot be overloaded");
    auto* function = dtorSet->front();
    return allocate<ast::LifetimeCall>(object,
                                       function,
                                       SpecialMemberFunction::Delete);
}

UniquePtr<ast::ExpressionStatement> sema::makeDestructorCallStmt(
    sema::Object* object) {
    if (auto destructorCall = makeDestructorCall(object)) {
        return allocate<ast::ExpressionStatement>(std::move(destructorCall));
    }
    return nullptr;
}
