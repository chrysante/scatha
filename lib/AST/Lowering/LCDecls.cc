#include "AST/Lowering/LoweringContext.h"

#include <range/v3/view.hpp>
#include <svm/Builtin.h>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

void LoweringContext::makeDeclarations() {
    arrayViewType = ctx.anonymousStructure(
        std::array<ir::Type const*, 2>{ ctx.pointerType(),
                                        ctx.integralType(64) });
    for (auto* type: symbolTable.sortedStructureTypes()) {
        declareType(type);
    }
    for (auto* function: symbolTable.functions()) {
        if (function->isNative()) {
            declareFunction(function);
        }
    }
}

void LoweringContext::declareType(sema::StructureType const* structType) {
    auto structure = allocate<ir::StructureType>(structType->mangledName());
    for (auto* member: structType->memberVariables()) {
        structure->addMember(mapType(member->type()));
    }
    typeMap[structType] = structure.get();
    mod.addStructure(std::move(structure));
}

ir::Callable* LoweringContext::declareFunction(sema::Function const* function) {
    auto paramTypes = [&] {
        if (function->isBuiltin()) {
            /// Some builtins need special care so we do that here
            if (function->index() == static_cast<size_t>(svm::Builtin::memcpy))
            {
                return utl::small_vector<ir::Type const*>{ ctx.pointerType(),
                                                           ctx.pointerType(),
                                                           ctx.integralType(
                                                               64) };
            }
        }
        return function->argumentTypes() |
               ranges::views::transform([this](sema::QualType const* qType) {
                   return mapType(qType);
               }) |
               ranges::to<utl::small_vector<ir::Type const*>>;
    }();

    // TODO: Generate proper function type here
    ir::FunctionType const* const functionType = nullptr;

    switch (function->kind()) {
    case sema::FunctionKind::Native: {
        auto fn               = allocate<ir::Function>(functionType,
                                         mapType(function->returnType()),
                                         paramTypes,
                                         function->mangledName(),
                                         mapFuncAttrs(function->attributes()),
                                         accessSpecToVisibility(
                                             function->accessSpecifier()));
        auto* result          = fn.get();
        functionMap[function] = result;
        mod.addFunction(std::move(fn));
        return result;
    }

    case sema::FunctionKind::External: {
        auto fn =
            allocate<ir::ExtFunction>(functionType,
                                      mapType(
                                          function->signature().returnType()),
                                      paramTypes,
                                      std::string(function->name()),
                                      utl::narrow_cast<uint32_t>(
                                          function->slot()),
                                      utl::narrow_cast<uint32_t>(
                                          function->index()),
                                      mapFuncAttrs(function->attributes()));
        auto* result          = fn.get();
        functionMap[function] = result;
        mod.addGlobal(std::move(fn));
        return result;
    }

    default:
        SC_UNREACHABLE();
    }
}