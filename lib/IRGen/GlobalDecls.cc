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

StructMetaData irgen::makeStructMetadata(sema::StructType const* semaType) {
    StructMetaData metadata;
    size_t irIndex = 0;
    for (auto* member: semaType->memberVariables()) {
        metadata.indexMap.push_back(utl::narrow_cast<uint16_t>(irIndex++));
        if (isFatPointer(member->type())) {
            ++irIndex;
        }
    }
    return metadata;
}

ir::StructType* irgen::generateType(sema::StructType const* semaType,
                                    ir::Context& ctx,
                                    ir::Module& mod,
                                    TypeMap& typeMap,
                                    sema::NameMangler const& nameMangler) {
    auto structType = allocate<ir::StructType>(nameMangler(*semaType));
    for (auto* member: semaType->memberVariables()) {
        structType->pushMember(typeMap(member->type()));
        /// Pointer to array data members need a second field in the IR struct
        /// to store the size of the array
        if (isFatPointer(member->type())) {
            structType->pushMember(ctx.intType(64));
        }
    }
    auto* result = structType.get();
    typeMap.insert(semaType, result, makeStructMetadata(semaType));
    mod.addStructure(std::move(structType));
    return result;
}

static bool isTrivial(sema::Type const* type) {
    return type->hasTrivialLifetime();
}

static size_t const maxRegPassingSize = 16;

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

FunctionMetaData irgen::makeFunctionMetadata(sema::Function const* semaFn) {
    return { .CC = computeCC(semaFn) };
}

namespace {

struct IRSignature {
    ir::Type const* returnType = nullptr;
    utl::small_vector<ir::Type const*> argumentTypes;
};

} // namespace

static IRSignature computeIRSignature(sema::Function const& semaFn,
                                      ir::Context& ctx,
                                      CallingConvention const& CC,
                                      TypeMap const& typeMap) {
    IRSignature sig;
    switch (CC.returnValue().location()) {
    case Register: {
        if (isFatPointer(semaFn.returnType())) {
            sig.returnType = makeArrayPtrType(ctx);
        }
        else {
            sig.returnType = typeMap(semaFn.returnType());
        }
        break;
    }
    case Memory:
        sig.returnType = ctx.voidType();
        sig.argumentTypes.push_back(ctx.ptrType());
        break;
    }
    for (auto [argPC, type]:
         ranges::views::zip(CC.arguments(), semaFn.argumentTypes()))
    {
        switch (argPC.location()) {
        case Register:
            sig.argumentTypes.push_back(typeMap(type));
            break;

        case Memory: {
            sig.argumentTypes.push_back(ctx.ptrType());
            break;
        }
        }
        /// Extremely hacky solution to add the size as a second argument.
        /// This works because the only case `argPC.numParams() == 2` is the
        /// dynamic array case.
        if (argPC.numParams() == 2) {
            sig.argumentTypes.push_back(ctx.intType(64));
        }
    }
    return sig;
}

ir::Callable* irgen::declareFunction(sema::Function const* semaFn,
                                     ir::Context& ctx,
                                     ir::Module& mod,
                                     TypeMap const& typeMap,
                                     FunctionMap& functionMap,
                                     sema::NameMangler const& nameMangler) {
    FunctionMetaData metaData = makeFunctionMetadata(semaFn);
    auto irSignature = computeIRSignature(*semaFn, ctx, metaData.CC, typeMap);
    UniquePtr<ir::Callable> irFn;
    switch (semaFn->kind()) {
    case sema::FunctionKind::Native:
        [[fallthrough]];
    case sema::FunctionKind::Generated: {
        irFn = allocate<ir::Function>(ctx,
                                      irSignature.returnType,
                                      makeParameters(irSignature.argumentTypes),
                                      nameMangler(*semaFn),
                                      mapFuncAttrs(semaFn->attributes()),
                                      mapVisibility(semaFn->accessControl()));
        break;
    }
    case sema::FunctionKind::Foreign: {
        irFn =
            allocate<ir::ForeignFunction>(ctx,
                                          irSignature.returnType,
                                          makeParameters(
                                              irSignature.argumentTypes),
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
