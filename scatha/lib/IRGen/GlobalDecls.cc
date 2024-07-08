#include "IRGen/GlobalDecls.h"

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "IR/Attributes.h"
#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/GlobalVariable.h"
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

static void generateVTable(sema::VTable const& vtable, RecordMetadata& MD,
                           LoweringContext lctx) {
    auto& ctx = lctx.ctx;
    std::string typeName = lctx.config.nameMangler(*vtable.correspondingType());
    utl::small_vector<ir::Constant*> vtableFunctions;
    auto dfs = [&](auto& dfs, sema::VTable const& vtable,
                   int level = 0) mutable -> void {
        if (level == 1) {
            MD.inheritedVTableOffsets.push_back(vtableFunctions.size());
        }
        for (auto* inherited: vtable.sortedInheritedVTables()) {
            dfs(dfs, *inherited, level + 1);
        }
        for (auto* semaFn: vtable.layout()) {
            auto* irFn = [&]() -> ir::Constant* {
                if (!semaFn->isAbstract()) {
                    return getFunction(*semaFn, lctx);
                }
                getFunction(*semaFn, lctx, /* pushToDeclQueue: */ false);
                return ctx.undef(ctx.ptrType());
            }();
            MD.vtableIndexMap.insert({ semaFn, vtableFunctions.size() });
            vtableFunctions.push_back(irFn);
        }
    };
    dfs(dfs, vtable);
    if (vtableFunctions.empty()) {
        return;
    }
    if (isa<sema::StructType>(vtable.correspondingType())) {
        auto* irVTable =
            ctx.arrayConstant(vtableFunctions,
                              ctx.arrayType(ctx.ptrType(),
                                            vtableFunctions.size()));
        MD.vtable = lctx.mod.addGlobal(
            allocate<ir::GlobalVariable>(ctx, ir::GlobalVariable::Const,
                                         irVTable, typeName + ".vtable"));
    }
}

static void traverseRecord(sema::RecordType const* record, auto callback) {
    for (auto* elem: record->elements()) {
        auto* elemType = elem->type();
        // clang-format off
        bool empty = SC_MATCH (*elemType) {
            [&](sema::RecordType const& type) {
                return isa<sema::BaseClassObject>(elem->asObject()) &&
                       type.isEmpty();
            },
            [](sema::Type const&) { return false; },
        }; // clang-format on
        callback(elemType, empty);
    }
}

/// Generates the lowering metadata for \p semaType
RecordMetadata irgen::makeRecordMetadata(sema::RecordType const* semaType,
                                         LoweringContext lctx) {
    RecordMetadata MD;
    uint32_t irIndex = 0;
    traverseRecord(semaType, [&](sema::Type const* type, bool empty) {
        auto& member = MD.members.push_back({ .beginIndex = irIndex });
        if (!empty) {
            member.fieldTypes = lctx.typeMap.unpacked(type);
            irIndex += member.fieldTypes.size();
        }
    });
    if (semaType->vtable()) {
        generateVTable(*semaType->vtable(), MD, lctx);
    }
    return MD;
}

ir::StructType* irgen::generateType(sema::RecordType const* semaType,
                                    LoweringContext lctx) {
    auto structType =
        allocate<ir::StructType>(lctx.config.nameMangler(*semaType));
    traverseRecord(semaType, [&](sema::Type const* type, bool empty) {
        if (empty) {
            return;
        }
        for (auto* irElemType: lctx.typeMap.unpacked(type)) {
            structType->pushMember(irElemType);
        }
    });
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
    if (semaFn.isAbstract()) {
        auto CC = computeCallingConvention(semaFn);
        lctx.globalMap.insert(&semaFn,
                              FunctionMetadata{ nullptr, std::move(CC) });
        return nullptr;
    }
    else {
        auto CC = computeCallingConvention(semaFn);
        auto irSignature =
            computeIRSignature(semaFn, lctx.ctx, CC, lctx.typeMap);
        auto irFn = allocateFunction(lctx.ctx, semaFn, CC, irSignature,
                                     lctx.typeMap, lctx.config.nameMangler);
        lctx.globalMap.insert(&semaFn,
                              FunctionMetadata{ irFn.get(), std::move(CC) });
        auto* result = irFn.get();
        lctx.mod.addGlobal(std::move(irFn));
        return result;
    }
}

ir::Callable* irgen::getFunction(sema::Function const& semaFn,
                                 LoweringContext lctx, bool pushToDeclQueue) {
    if (auto md = lctx.globalMap.tryGet(&semaFn)) {
        return md->function;
    }
    if (pushToDeclQueue && (semaFn.isNative() || semaFn.isGenerated()) &&
        !semaFn.isAbstract() && !lctx.lowered.contains(&semaFn))
    {
        lctx.declQueue.push_back(&semaFn);
        lctx.lowered.insert(&semaFn);
    }
    return declareFunction(semaFn, lctx);
}
