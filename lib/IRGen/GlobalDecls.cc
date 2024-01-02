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
#include "Sema/NameMangling.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace irgen;
using enum ValueLocation;
using sema::QualType;

ir::StructType* irgen::generateType(sema::StructType const* semaType,
                                    ir::Context& ctx,
                                    ir::Module& mod,
                                    TypeMap& typeMap,
                                    sema::NameMangler const& nameMangler) {
    auto structType = allocate<ir::StructType>(nameMangler(*semaType));
    StructMetaData metaData;
    size_t irIndex = 0;
    for (auto* member: semaType->memberVariables()) {
        structType->pushMember(typeMap(member->type()));
        metaData.indexMap.push_back(utl::narrow_cast<uint16_t>(irIndex++));
        /// Pointer to array data members need a second field in the IR struct
        /// to store the size of the array
        if (isFatPointer(member->type())) {
            structType->pushMember(ctx.intType(64));
            ++irIndex;
        }
    }
    auto* result = structType.get();
    typeMap.insert(semaType, result, std::move(metaData));
    mod.addStructure(std::move(structType));
    return result;
}

static bool isTrivial(sema::Type const* type) {
    return type->hasTrivialLifetime();
}

static const size_t maxRegPassingSize = 16;

static PassingConvention computePCImpl(sema::Type const* type, bool isRetval) {
    if (isFatPointer(type)) {
        return PassingConvention(Register, isRetval ? 0 : 2);
    }
    bool const isSmall = isa<sema::ReferenceType>(type) ||
                         type->size() <= maxRegPassingSize;
    if (isSmall && isTrivial(type)) {
        return PassingConvention(Register, isRetval ? 0u : 1u);
    }
    return PassingConvention(Memory, 1);
}

static PassingConvention computeRetValPC(sema::Type const* type) {
    if (isa<sema::VoidType>(type)) {
        return { Register, 0 };
    }
    return computePCImpl(type, true);
}

static PassingConvention computeArgPC(sema::Type const* type) {
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
                                     FunctionMap& functionMap,
                                     sema::NameMangler const& nameMangler) {
    FunctionMetaData metaData;
    auto CC = computeCC(semaFn);
    metaData.CC = CC;
    ir::Type const* irReturnType = nullptr;
    utl::small_vector<ir::Type const*> irArgTypes;
    auto retvalPC = CC.returnValue();
    switch (retvalPC.location()) {
    case Register: {
        if (isFatPointer(semaFn->returnType())) {
            irReturnType = makeArrayViewType(ctx);
        }
        else {
            irReturnType = typeMap(semaFn->returnType());
        }
        break;
    }
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

    UniquePtr<ir::Callable> irFn;
    switch (semaFn->kind()) {
    case sema::FunctionKind::Native:
        [[fallthrough]];
    case sema::FunctionKind::Generated: {
        irFn =
            allocate<ir::Function>(ctx,
                                   irReturnType,
                                   irArgTypes,
                                   nameMangler(*semaFn),
                                   mapFuncAttrs(semaFn->attributes()),
                                   mapVisibility(semaFn->binaryVisibility()));
        break;
    }
    case sema::FunctionKind::Foreign: {
        irFn =
            allocate<ir::ForeignFunction>(ctx,
                                          irReturnType,
                                          irArgTypes,
                                          std::string(semaFn->name()),
                                          mapFuncAttrs(semaFn->attributes()));
        break;
    }
    }
    functionMap.insert(semaFn, irFn.get(), std::move(metaData));
    auto* result = irFn.get();
    mod.addGlobal(std::move(irFn));
    return result;
}
