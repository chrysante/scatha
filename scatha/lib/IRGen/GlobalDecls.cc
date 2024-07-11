#include "IRGen/GlobalDecls.h"

#include <range/v3/view.hpp>
#include <utl/function_view.hpp>
#include <utl/stack.hpp>
#include <utl/utility.hpp>

#include "IR/Attributes.h"
#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
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

static std::string makeVTableName(sema::VTable const& vt,
                                  sema::NameMangler const& nameMangler) {
    std::string typeName = nameMangler(*vt.correspondingType());
    return typeName + ".vtable";
}

static std::string makeThunkName(ir::Function* target,
                                 sema::RecordType const* concreteType,
                                 sema::NameMangler const& nameMangler) {
    return utl::strcat(target->name(), ".thunk-for-",
                       nameMangler(*concreteType));
}

static ir::Function* generateThunk(ir::Function* target,
                                   sema::RecordType const* concreteType,
                                   ssize_t objectByteOffset,
                                   ssize_t vtableOffset, ir::Visibility vis,
                                   LoweringContext& lctx) {
    auto& ctx = lctx.ctx;
    auto name = makeThunkName(target, concreteType, lctx.config.nameMangler);
    auto owner = allocate<ir::Function>(ctx, target->returnType(),
                                        ir::clone(ctx, target->parameters()),
                                        std::move(name), target->attributes(),
                                        vis);
    auto* thunk = lctx.mod.addGlobal(std::move(owner));
    ir::FunctionBuilder builder(ctx, thunk);
    builder.addNewBlock("entry");
    SC_ASSERT(ranges::distance(thunk->parameters()) >= 2,
              "Must have at least object pointer and vtable pointer");
    ir::Value* objPtr = thunk->parameters().begin().to_address();
    ir::Value* vtablePtr = std::next(thunk->parameters().begin()).to_address();
    if (objectByteOffset > 0) {
        objPtr = builder.add<ir::GetElementPointer>(
            ctx.intType(8), objPtr, ctx.intConstant(-objectByteOffset, 64),
            std::span<size_t>{}, "objptr.offset");
    }
    if (vtableOffset > 0) {
        vtablePtr =
            builder.add<ir::GetElementPointer>(ctx.ptrType(), vtablePtr,
                                               ctx.intConstant(-vtableOffset,
                                                               64),
                                               std::span<size_t>{},
                                               "vtable.offset");
    }
    utl::small_vector<ir::Value*> args = { objPtr, vtablePtr };
    args.insert(args.end(), thunk->parameters() | drop(2) | TakeAddress);
    auto* result =
        builder.add<ir::Call>(target->returnType(), target, args, "result");
    builder.add<ir::Return>(result);
    return thunk;
}

namespace {

struct DFSStackElem {
    sema::RecordType const* type;
    size_t vtableOffset;
};

} // namespace

static ir::Visibility max(ir::Visibility a, ir::Visibility b) {
    using enum ir::Visibility;
    if (a == External || b == External) {
        return External;
    }
    return Internal;
}

namespace {

struct VTableGenCallbacks {
    utl::function_view<ir::Function*(sema::Function const& concreteSemaFn,
                                     sema::RecordType const* owningType,
                                     ir::Function* target)>
        thunkCache;

    utl::function_view<ir::Function*(sema::Function const& concreteSemaFn,
                                     sema::RecordType const* owningType,
                                     ir::Function* target, ssize_t byteOffset,
                                     ssize_t vtableOffset, ir::Visibility)>
        thunkGen;

    utl::function_view<ir::GlobalVariable*(std::span<ir::Constant* const>,
                                           ir::Visibility)>
        vtableFactory;
};

} // namespace

static ir::Constant* findOrGenVTableFn(sema::Function const& concreteSemaFn,
                                       sema::RecordType const* owningVTableType,
                                       std::span<DFSStackElem const> dfsStack,
                                       ir::Visibility vis,
                                       VTableGenCallbacks callbacks,
                                       LoweringContext& lctx) {
    auto& ctx = lctx.ctx;
    if (concreteSemaFn.isAbstract()) {
        (void)getFunction(concreteSemaFn, lctx, /* pushToDeclQueue: */ false);
        return ctx.undef(ctx.ptrType());
    }
    auto* target = cast<ir::Function*>(getFunction(concreteSemaFn, lctx));
    vis = max(vis, target->visibility());
    auto* fnObjType = cast<sema::RecordType const*>(concreteSemaFn.parent());
    auto owningTypeItr =
        ranges::find(dfsStack, owningVTableType, &DFSStackElem::type);
    SC_ASSERT(owningTypeItr != dfsStack.end(), "");
    auto concreteTypeItr =
        ranges::find(dfsStack, fnObjType, &DFSStackElem::type);
    SC_ASSERT(concreteTypeItr != dfsStack.end(), "");
    if (owningTypeItr == concreteTypeItr) {
        target->setVisibility(vis);
        return target;
    }
    if (auto* thunk =
            callbacks.thunkCache(concreteSemaFn, owningTypeItr->type, target))
    {
        return thunk;
    }
    size_t byteOffset = 0;
    auto* currRecord = concreteTypeItr->type;
    auto castSequence = ranges::make_subrange(std::next(concreteTypeItr),
                                              std::next(owningTypeItr));
    for (auto& [type, vtOffset]: castSequence) {
        auto itr = ranges::find(currRecord->baseObjects(), type,
                                &sema::BaseClassObject::type);
        SC_ASSERT(itr != currRecord->baseObjects().end(), "");
        byteOffset += (*itr)->byteOffset();
        currRecord = type;
    }
    SC_ASSERT(
        owningTypeItr->vtableOffset >= concreteTypeItr->vtableOffset,
        "The owning type must be further into the vtable than the concrete type");
    size_t vtableOffset =
        owningTypeItr->vtableOffset - concreteTypeItr->vtableOffset;
    if (byteOffset == 0 && vtableOffset == 0) {
        return target;
    }
    return callbacks.thunkGen(concreteSemaFn, owningTypeItr->type, target,
                              utl::narrow_cast<ssize_t>(byteOffset),
                              utl::narrow_cast<ssize_t>(vtableOffset), vis);
}

static void generateVTable(sema::VTable const& vtable, RecordMetadata& MD,
                           VTableGenCallbacks callbacks,
                           LoweringContext& lctx) {
    auto visibility = mapVisibility(vtable.correspondingType());
    utl::small_vector<ir::Constant*> vtableFunctions;
    utl::stack<DFSStackElem> dfsStack;
    auto dfs = [&](auto& dfs, sema::VTable const& vtable,
                   int level = 0) -> void {
        dfsStack.push({ .type = vtable.correspondingType(),
                        .vtableOffset = vtableFunctions.size() });
        if (level == 1) {
            MD.inheritedVTableOffsets.push_back(dfsStack.top().vtableOffset);
        }
        for (auto* inherited: vtable.sortedInheritedVTables()) {
            dfs(dfs, *inherited, level + 1);
        }
        for (auto* semaFn: vtable.layout()) {
            auto* irFn = findOrGenVTableFn(*semaFn, vtable.correspondingType(),
                                           dfsStack, visibility, callbacks,
                                           lctx);
            MD.vtableIndexMap.insert({ semaFn, vtableFunctions.size() });
            vtableFunctions.push_back(irFn);
        }
        dfsStack.pop();
    };
    dfs(dfs, vtable);
    if (!vtableFunctions.empty() &&
        isa<sema::StructType>(vtable.correspondingType()))
    {
        MD.vtable = callbacks.vtableFactory(vtableFunctions, visibility);
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

static RecordMetadata makeRecordMetadataImpl(sema::RecordType const* semaType,
                                             VTableGenCallbacks callbacks,
                                             LoweringContext& lctx) {
    RecordMetadata MD;
    uint32_t irIndex = 0;
    traverseRecord(semaType, [&](sema::Type const* type, bool empty) {
        auto& member = MD.members.push_back({ .beginIndex = irIndex });
        if (!empty) {
            member.fieldTypes = lctx.typeMap.unpacked(type);
            irIndex += member.fieldTypes.size();
        }
    });
    if (auto* vtable = semaType->vtable()) {
        generateVTable(*vtable, MD, callbacks, lctx);
    }
    return MD;
}

/// Generates the lowering metadata for \p semaType
static RecordMetadata makeRecordMetadata(sema::RecordType const* semaType,
                                         LoweringContext& lctx) {
    auto& ctx = lctx.ctx;
    auto* vtable = semaType->vtable();
    auto thunkCache = [&](sema::Function const& concreteSemaFn,
                          sema::RecordType const* owningType, ir::Function*) {
        auto itr = lctx.thunkMap.find({ &concreteSemaFn, owningType });
        return itr != lctx.thunkMap.end() ? itr->second : nullptr;
    };
    auto thunkGen = [&](sema::Function const& concreteSemaFn,
                        sema::RecordType const* owningType,
                        ir::Function* target, ssize_t byteOffset,
                        ssize_t vtableOffset, ir::Visibility vis) {
        SC_EXPECT(byteOffset != 0 || vtableOffset != 0);
        auto* objType = cast<sema::RecordType const*>(concreteSemaFn.parent());
        auto* thunk =
            generateThunk(target, objType, byteOffset, vtableOffset, vis, lctx);
        ThunkKey key = { &concreteSemaFn, owningType };
        lctx.thunkMap.insert({ key, thunk });
        return thunk;
    };
    auto vtCallback = [&](std::span<ir::Constant* const> functions,
                          ir::Visibility vis) {
        SC_EXPECT(vtable);
        auto* irVTable =
            ctx.arrayConstant(functions,
                              ctx.arrayType(ctx.ptrType(), functions.size()));
        std::string name = makeVTableName(*vtable, lctx.config.nameMangler);
        return lctx.mod.addGlobal(
            allocate<ir::GlobalVariable>(ctx, ir::GlobalVariable::Const,
                                         irVTable, std::move(name), vis));
    };
    return makeRecordMetadataImpl(semaType,
                                  { thunkCache, thunkGen, vtCallback }, lctx);
}

/// Generates the lowering metadata for \p semaType
RecordMetadata irgen::makeRecordMetadataImport(sema::RecordType const* semaType,
                                               ImportMap const& importMap,
                                               LoweringContext& lctx) {
    auto* vtable = semaType->vtable();
    auto thunkCache = [&](sema::Function const& concreteSemaFn,
                          sema::RecordType const*, ir::Function* target) {
        auto* concreteType =
            cast<sema::RecordType const*>(concreteSemaFn.parent());
        std::string name =
            makeThunkName(target, concreteType, lctx.config.nameMangler);
        return importMap.tryGet<ir::Function>(name);
    };
    auto thunkGen = [&](sema::Function const& concreteSemaFn,
                        sema::RecordType const*, ir::Function* target,
                        ssize_t byteOffset, ssize_t vtableOffset,
                        ir::Visibility) {
        SC_EXPECT(byteOffset != 0 || vtableOffset != 0);
        auto* concreteType =
            cast<sema::RecordType const*>(concreteSemaFn.parent());
        std::string name =
            makeThunkName(target, concreteType, lctx.config.nameMangler);
        return importMap.get<ir::Function>(name);
    };
    auto vtCallback = [&](std::span<ir::Constant* const>, ir::Visibility) {
        std::string name = makeVTableName(*vtable, lctx.config.nameMangler);
        return importMap.get<ir::GlobalVariable>(name);
    };
    return makeRecordMetadataImpl(semaType,
                                  { thunkCache, thunkGen, vtCallback }, lctx);
}

ir::StructType* irgen::generateType(sema::RecordType const* semaType,
                                    LoweringContext& lctx) {
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
                                     LoweringContext& lctx) {
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
                                 LoweringContext& lctx, bool pushToDeclQueue) {
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
