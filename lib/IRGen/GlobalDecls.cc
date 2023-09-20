#include "IRGen/GlobalDecls.h"

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IRGen/CallingConvention.h"
#include "IRGen/Maps.h"
#include "IRGen/MetaData.h"
#include "IRGen/Utility.h"
#include "Sema/Entity.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace irgen;
using enum ValueLocation;
using sema::QualType;

static sema::Type const* getPtrOrRefBase(sema::Type const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [](sema::ReferenceType const& type) {
            return type.base().get();
        },
        [](sema::PointerType const& type) {
            return type.base().get();
        },
        [](sema::Type const& type) {
            return &type;
        }
    }; // clang-format on
}

static bool memberNeedsSizeField(sema::Type const* member) {
    if (!isa<sema::ReferenceType>(member) && !isa<sema::PointerType>(member)) {
        return false;
    }
    auto* array = dyncast<sema::ArrayType const*>(getPtrOrRefBase(member));
    return array && array->isDynamic();
}

ir::StructType* irgen::generateType(sema::StructType const* semaType,
                                    ir::Context& ctx,
                                    ir::Module& mod,
                                    TypeMap& typeMap) {
    auto structType = allocate<ir::StructType>(semaType->mangledName());
    StructMetaData metaData;
    size_t irIndex = 0;
    for (auto* member: semaType->memberVariables()) {
        sema::QualType memType = member->type();
        structType->pushMember(typeMap(memType));
        metaData.indexMap.push_back(utl::narrow_cast<uint16_t>(irIndex++));
        if (memberNeedsSizeField(memType.get())) {
            structType->pushMember(ctx.intType(64));
            ++irIndex;
        }
    }
    auto* result = structType.get();
    typeMap.insert(semaType, result, std::move(metaData));
    mod.addStructure(std::move(structType));
    return result;
}

static bool isTrivial(sema::QualType type) {
    return type->hasTrivialLifetime();
}

static const size_t maxRegPassingSize = 16;

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

ir::Callable* irgen::declareFunction(sema::Function const* semaFn,
                                     ir::Context& ctx,
                                     ir::Module& mod,
                                     TypeMap const& typeMap,
                                     FunctionMap& functionMap) {
    FunctionMetaData metaData;
    auto CC = computeCC(semaFn);
    metaData.CC = CC;
    ir::Type const* irReturnType = nullptr;
    utl::small_vector<ir::Type const*> irArgTypes;
    auto retvalPC = CC.returnValue();
    switch (retvalPC.location()) {
    case Register:
        if (isArrayPtrOrArrayRef(semaFn->returnType().get())) {
            irReturnType = makeArrayViewType(ctx);
        }
        else {
            irReturnType = typeMap(semaFn->returnType());
        }
        break;

    case Memory:
        irReturnType = ctx.voidType();
        irArgTypes.push_back(ctx.ptrType());
        break;
    }
    for (auto [argPC, type]:
         ranges::views::zip(CC.arguments(), semaFn->argumentTypes()))
    {
        switch (argPC.location()) {
        case Register:
            irArgTypes.push_back(typeMap(type));
            break;

        case Memory: {
            irArgTypes.push_back(ctx.ptrType());
            break;
        }
        }
        /// Extremely hacky solution to add the size as a second argument.
        /// This works because the only case `argPC.numParams() == 2` is the
        /// dynamic array case.
        if (argPC.numParams() == 2) {
            irArgTypes.push_back(ctx.intType(64));
        }
    }

    // TODO: Generate proper function type here
    ir::FunctionType const* const functionType = nullptr;
    UniquePtr<ir::Callable> irFn;
    switch (semaFn->kind()) {
    case sema::FunctionKind::Native: {
        irFn =
            allocate<ir::Function>(functionType,
                                   irReturnType,
                                   irArgTypes,
                                   semaFn->mangledName(),
                                   mapFuncAttrs(semaFn->attributes()),
                                   mapVisibility(semaFn->binaryVisibility()));
        break;
    }
    case sema::FunctionKind::Foreign: {
        irFn =
            allocate<ir::ForeignFunction>(functionType,
                                          irReturnType,
                                          irArgTypes,
                                          std::string(semaFn->name()),
                                          utl::narrow_cast<uint32_t>(
                                              semaFn->slot()),
                                          utl::narrow_cast<uint32_t>(
                                              semaFn->index()),
                                          mapFuncAttrs(semaFn->attributes()));
        break;
    }

    case sema::FunctionKind::Generated:
        SC_UNIMPLEMENTED();
    }
    functionMap.insert(semaFn, irFn.get(), std::move(metaData));
    auto* result = irFn.get();
    mod.addGlobal(std::move(irFn));
    return result;
}
