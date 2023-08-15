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

using enum ValueLocation;

void LoweringContext::makeDeclarations() {
    arrayViewType = ctx.anonymousStructure(
        std::array<ir::Type const*, 2>{ ctx.pointerType(),
                                        ctx.integralType(64) });
    for (auto* type: analysisResult.structDependencyOrder) {
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

static bool isTrivial(sema::Type const* type) {
    /// For now all types are trivial since we don't have constructors and
    /// destructors yet
    return true;
}

static const size_t maxRegPassingSize = 16;

static PassingConvention computePCImpl(sema::QualType const* type,
                                       bool isRetval) {
    if (isTrivial(type) && type->size() <= maxRegPassingSize) {
        return { Register, isRetval ? 0u : 1u };
    }
    /// When we support arrays we need 2 here
    return { Memory, 1 };
}

static PassingConvention computeRetValPC(sema::QualType const* type) {
    if (isa<sema::VoidType>(type->base())) {
        return { Register, 0 };
    }
    return computePCImpl(type, true);
}

static PassingConvention computeArgPC(sema::QualType const* type) {
    return computePCImpl(type, false);
}

static CallingConvention computeCC(sema::Function const* function) {
    auto retval = computeRetValPC(function->returnType());
    auto args = function->argumentTypes() |
                ranges::views::transform(computeArgPC) |
                ranges::to<utl::small_vector<PassingConvention>>;
    return CallingConvention(retval, args);
}

ir::Callable* LoweringContext::declareFunction(sema::Function const* function) {
    auto CC = computeCC(function);
    CCMap[function] = CC;
    ir::Type const* irReturnType = nullptr;
    utl::small_vector<ir::Type const*> irArgTypes;
    auto retvalPC = CC.returnValue();
    switch (retvalPC.location()) {
    case Register:
        irReturnType = mapType(function->returnType());
        break;

    case Memory:
        irReturnType = ctx.voidType();
        irArgTypes.push_back(ctx.pointerType());
        break;
    }
    for (auto [argPC, type]:
         ranges::views::zip(CC.arguments(), function->argumentTypes()))
    {
        switch (argPC.location()) {
        case Register:
            irArgTypes.push_back(mapType(type));
            break;

        case Memory:
            irArgTypes.push_back(ctx.pointerType());
            break;
        }
    }

    // TODO: Generate proper function type here
    ir::FunctionType const* const functionType = nullptr;

    switch (function->kind()) {
    case sema::FunctionKind::Native: {
        auto fn = allocate<ir::Function>(functionType,
                                         irReturnType,
                                         irArgTypes,
                                         function->mangledName(),
                                         mapFuncAttrs(function->attributes()),
                                         accessSpecToVisibility(
                                             function->accessSpecifier()));
        auto* result = fn.get();
        functionMap[function] = result;
        mod.addFunction(std::move(fn));
        return result;
    }

    case sema::FunctionKind::External: {
        auto fn =
            allocate<ir::ExtFunction>(functionType,
                                      irReturnType,
                                      irArgTypes,
                                      std::string(function->name()),
                                      utl::narrow_cast<uint32_t>(
                                          function->slot()),
                                      utl::narrow_cast<uint32_t>(
                                          function->index()),
                                      mapFuncAttrs(function->attributes()));
        auto* result = fn.get();
        functionMap[function] = result;
        mod.addGlobal(std::move(fn));
        return result;
    }

    default:
        SC_UNREACHABLE();
    }
}
