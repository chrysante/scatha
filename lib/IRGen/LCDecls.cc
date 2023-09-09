#include "IRGen/LoweringContext.h"

#include <range/v3/view.hpp>
#include <svm/Builtin.h>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

using sema::QualType;

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
    size_t irIndex = 0;
    for (auto [semaIndex, member]:
         structType->memberVariables() | ranges::views::enumerate)
    {
        QualType memType = member->type();
        structure->addMember(mapType(memType));
        structIndexMap[{ structType, semaIndex }] = irIndex++;
        auto* arrayType = dyncast<sema::ArrayType const*>(
            sema::stripReference(memType).get());
        if (!arrayType || !arrayType->isDynamic()) {
            continue;
        }
        SC_ASSERT(sema::isRef(memType), "Can't have dynamic arrays in structs");
        structure->addMember(ctx.integralType(64));
        /// We simply increment the index without adding anything to the map
        /// because `getValueImpl(MemberAccess)` will know what to do
        ++irIndex;
    }
    typeMap[structType] = structure.get();
    mod.addStructure(std::move(structure));
}

static bool isTrivial(sema::QualType type) {
    return type->hasTrivialLifetime();
}

static const size_t maxRegPassingSize = 16;

static bool isArrayAndDynamic(sema::ObjectType const* type) {
    auto* arrayType = dyncast<sema::ArrayType const*>(type);
    return arrayType && arrayType->isDynamic();
}

static PassingConvention computePCImpl(sema::QualType type, bool isRetval) {
    if (auto* refType = dyncast<sema::RefTypeBase const*>(type.get())) {
        size_t argCount = isArrayAndDynamic(refType->base().get()) ? 2 : 1;
        return PassingConvention(Register, isRetval ? 0 : argCount);
    }
    bool const isSmall = type->size() <= maxRegPassingSize;
    if (isSmall && isTrivial(type)) {
        return PassingConvention(Register, isRetval ? 0u : 1u);
    }
    size_t argCount = isArrayAndDynamic(type.get()) ? 2 : 1;
    return PassingConvention(Memory, argCount);
}

static PassingConvention computeRetValPC(sema::QualType type) {
    if (isa<sema::VoidType>(*type)) {
        return { Register, 0 };
    }
    return computePCImpl(type, true);
}

static PassingConvention computeArgPC(sema::QualType type) {
    return computePCImpl(type, false);
}

static CallingConvention computeCC(sema::Function const* function) {
    auto retval = computeRetValPC(function->returnType());
    auto args = function->argumentTypes() |
                ranges::views::transform(computeArgPC) | ToSmallVector<>;
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
        switch (sema::stripReference(function->returnType())->entityType()) {
        case sema::EntityType::ArrayType: {
            irReturnType = arrayViewType;
            break;
        }
        default:
            irReturnType = mapType(function->returnType());
            break;
        }
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

        case Memory: {
            irArgTypes.push_back(ctx.pointerType());
            break;
        }
        }
        /// Extremely hacky solution to add the size as a second argument.
        /// This works because the only case `argPC.numParams() == 2` is the
        /// dynamic array case.
        if (argPC.numParams() == 2) {
            irArgTypes.push_back(ctx.integralType(64));
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

    case sema::FunctionKind::Generated: {
        SC_UNIMPLEMENTED();
    }

    default:
        SC_UNREACHABLE();
    }
}
