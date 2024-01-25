#include "IRGen/GlobalDecls.h"

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "IR/CFG/Function.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IRGen/CallingConvention.h"
#include "IRGen/Maps.h"
#include "IRGen/Metadata.h"
#include "IRGen/Utility.h"
#include "Sema/Entity.h"
#include "Sema/NameMangling.h"
#include "Sema/QualType.h"

using namespace scatha;
using namespace irgen;
using enum ValueLocation;
using enum ValueRepresentation;
using sema::QualType;

StructMetadata irgen::makeStructMetadata(TypeMap& typeMap,
                                         sema::StructType const* semaType) {
    StructMetadata metadata;
    uint32_t irIndex = 0;
    for (auto* member: semaType->memberVariables()) {
        auto fieldTypes = typeMap.unpacked(member->type());
        metadata.members.push_back(
            { .beginIndex = irIndex, .fieldTypes = fieldTypes });
        irIndex += fieldTypes.size();
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
        for (auto* irElemType: typeMap.unpacked(member->type())) {
            structType->pushMember(irElemType);
        }
    }
    auto* result = structType.get();
    typeMap.insert(semaType, result, makeStructMetadata(typeMap, semaType));
    mod.addStructure(std::move(structType));
    return result;
}

/// Only certain types like builtin arithmetic types, pointers, bools or struct
/// types with trivial lifetime may be passed in registers
static bool mayPassInRegister(sema::Type const* type) {
    /// `void` is symbolically passed in a register. Void is only valid for
    /// return values in this only means we don't allocate a function parameter
    /// for the return value
    if (isa<sema::VoidType>(type)) {
        return true;
    }
    if (type->size() > PreferredMaxRegisterValueSize) {
        return false;
    }
    /// For now! This is not necessarily correct for unique pointers
    return type->hasTrivialLifetime();
}

static PassingConvention computePCImpl(sema::Type const* type, bool isRetval) {
    if (mayPassInRegister(type)) {
        return { type, Register, isRetval ? 0u : isFatPointer(type) ? 2u : 1u };
    }
    else {
        return { type, Memory, 1 };
    }
}

static PassingConvention computeRetValPC(sema::Type const* type) {
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

FunctionMetadata irgen::makeFunctionMetadata(sema::Function const* semaFn) {
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
    /// Compute return value
    switch (CC.returnValue().location()) {
    case Register: {
        sig.returnType = typeMap.packed(semaFn.returnType());
        break;
    }
    case Memory:
        sig.returnType = ctx.voidType();
        sig.argumentTypes.push_back(ctx.ptrType());
        break;
    }
    /// Compute arguments
    for (auto [argPC, semaType]:
         ranges::views::zip(CC.arguments(), semaFn.argumentTypes()))
    {
        switch (argPC.location()) {
        case Register:
            for (auto* irType: typeMap.unpacked(semaType)) {
                sig.argumentTypes.push_back(irType);
            }
            break;

        case Memory: {
            sig.argumentTypes.push_back(ctx.ptrType());
            break;
        }
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
    FunctionMetadata metaData = makeFunctionMetadata(semaFn);
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
                                      mapVisibility(semaFn));
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
