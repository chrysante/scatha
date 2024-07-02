#include "IRGen/GlobalDecls.h"

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "IR/Attributes.h"
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
#include "Sema/VTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;
using enum ValueLocation;
using enum ValueRepresentation;
using sema::QualType;

static ir::GlobalVariable* generateVTable(sema::VTable const& vtable,
                                          LoweringContext lctx) {
    //    SC_UNIMPLEMENTED();
    return nullptr;
#if 0
    auto vtableType = allocate<ir::StructType>(nameMangler(*vtable.type()) + ".vtable");
    auto dfs = [&, index = size_t{0}](auto& dfs, sema::VTable const& vtable) mutable -> void {
        for (auto* inherited : vtable.sortedInheritedVTables()) {
            dfs(dfs, *inherited);
        }
        for (auto* F: vtable.layout()) {
            
        }
    };
    dfs(dfs, vtable);
#endif
}

/// Generates the lowering metadata for \p semaType
RecordMetadata irgen::makeRecordMetadata(sema::RecordType const* semaType,
                                         LoweringContext lctx) {
    RecordMetadata metadata;
    if (auto* S = dyncast<sema::StructType const*>(semaType)) {
        uint32_t irIndex = 0;
        for (auto* member: S->memberVariables()) {
            auto fieldTypes = lctx.typeMap.unpacked(member->type());
            metadata.members.push_back(
                { .beginIndex = irIndex, .fieldTypes = fieldTypes });
            irIndex += fieldTypes.size();
        }
    }
    if (auto* vtable = semaType->vtable()) {
        metadata.vtable = generateVTable(*vtable, lctx);
    }
    return metadata;
}

ir::StructType* irgen::generateType(sema::RecordType const* semaType,
                                    LoweringContext lctx) {
    auto structType =
        allocate<ir::StructType>(lctx.config.nameMangler(*semaType));
    if (auto* semaStruct = dyncast<sema::StructType const*>(semaType)) {
        auto impl = [&](sema::Object const* obj) {
            for (auto* irElemType: lctx.typeMap.unpacked(obj->type())) {
                structType->pushMember(irElemType);
            }
        };
        for (auto* base: semaStruct->baseObjects()) {
            if (!isa<sema::ProtocolType>(base->type()) &&
                base->type()->size() > 0)
            {
                impl(base);
            }
        }
        for (auto* var: semaStruct->memberVariables()) {
            impl(var);
        }
    }
    auto MD = makeRecordMetadata(semaType, lctx);
    auto* result = structType.get();
    lctx.typeMap.insert(semaType, result, std::move(MD));
    lctx.mod.addStructure(std::move(structType));
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
    /// Unique pointers are nontrivial but are passed in registers. They are
    /// destroyed inline and require no address stability
    if (isa<sema::UniquePtrType>(type)) {
        return true;
    }
    if (type->size() > PreferredMaxRegisterValueSize) {
        return false;
    }
    return type->hasTrivialLifetime();
}

static ValueLocation computeRetValLocation(sema::Type const* type) {
    return mayPassInRegister(type) ? Register : Memory;
}

static PassingConvention computeArgPC(sema::Type const* type) {
    if (mayPassInRegister(type)) {
        if (isDynArrayPointer(type) || isDynArrayReference(type) ||
            isDynPointer(type) || isDynReference(type))
        {
            return PassingConvention(type, { Register, Register });
        }
        return PassingConvention(type, { Register });
    }
    else {
        return PassingConvention(type, { Memory });
    }
}

CallingConvention irgen::computeCallingConvention(
    sema::Function const& function) {
    auto retvalLoc = computeRetValLocation(function.returnType());
    auto args = function.argumentTypes() |
                ranges::views::transform(computeArgPC) | ToSmallVector<>;
    return CallingConvention(function.returnType(), retvalLoc, args);
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
    if (CC.returnLocation() == Register) {
        sig.returnType = typeMap.packed(semaFn.returnType());
    }
    else {
        sig.returnType = ctx.voidType();
        sig.argumentTypes.push_back(ctx.ptrType());
    }
    /// Compute arguments
    for (auto [argPC, semaType]:
         ranges::views::zip(CC.arguments(), semaFn.argumentTypes()))
    {
        switch (argPC.location(0)) {
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

static List<ir::Parameter> makeParameters(CallingConvention const& CC,
                                          IRSignature const& irSig,
                                          TypeMap const& typemap) {
    auto params = makeParameters(irSig.argumentTypes);
    bool isValRet = CC.returnLocation() == ValueLocation::Memory;
    if (isValRet) {
        params.front().addAttribute<ir::ValRetAttribute>(
            typemap.packed(CC.returnType()));
    }
    for (auto [param, PC]: zip(params | drop(isValRet ? 1 : 0), CC.arguments()))
    {
        if (PC.numParams() == 1 && PC.location(0) == ValueLocation::Memory) {
            param.addAttribute<ir::ByValAttribute>(typemap.packed(PC.type()));
        }
    }
    return params;
}

static UniquePtr<ir::Callable> allocateFunction(
    ir::Context& ctx, sema::Function const& semaFn, CallingConvention const& CC,
    IRSignature const& irSig, TypeMap const& typemap,
    sema::NameMangler const& nameMangler) {
    auto params = makeParameters(CC, irSig, typemap);
    switch (semaFn.kind()) {
    case sema::FunctionKind::Native:
        [[fallthrough]];
    case sema::FunctionKind::Generated:
        return allocate<ir::Function>(ctx, irSig.returnType, std::move(params),
                                      nameMangler(semaFn),
                                      mapFuncAttrs(semaFn.attributes()),
                                      mapVisibility(&semaFn));
    case sema::FunctionKind::Foreign:
        return allocate<ir::ForeignFunction>(ctx, irSig.returnType,
                                             std::move(params),
                                             std::string(semaFn.name()),
                                             mapFuncAttrs(semaFn.attributes()));
    }
}

ir::Callable* irgen::declareFunction(sema::Function const& semaFn,
                                     LoweringContext lctx) {
    auto CC = computeCallingConvention(semaFn);
    auto irSignature = computeIRSignature(semaFn, lctx.ctx, CC, lctx.typeMap);
    auto irFn = allocateFunction(lctx.ctx, semaFn, CC, irSignature,
                                 lctx.typeMap, lctx.config.nameMangler);
    lctx.globalMap.insert(&semaFn,
                          FunctionMetadata{ irFn.get(), std::move(CC) });
    auto* result = irFn.get();
    lctx.mod.addGlobal(std::move(irFn));
    return result;
}
