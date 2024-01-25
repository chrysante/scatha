#include "IRGen/FunctionGeneration.h"

#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Context.h"
#include "IR/InvariantSetup.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;
using enum ValueLocation;
using enum ValueRepresentation;

void irgen::generateFunction(Config config, FuncGenParameters params) {
    if (params.semaFn.isNative()) {
        generateAstFunction(config, params);
    }
    else {
        SC_EXPECT(params.semaFn.isGenerated());
        generateSynthFunction(config, params);
    }
    ir::setupInvariants(params.ctx, params.irFn);
}

FuncGenContextBase::FuncGenContextBase(Config config, FuncGenParameters params):
    FuncGenParameters(params),
    FunctionBuilder(params.ctx, &params.irFn),
    config(config),
    valueMap(ctx),
    arrayPtrType(makeArrayPtrType(ctx)) {}

ir::Callable* FuncGenContextBase::getFunction(
    sema::Function const* semaFunction) {
    if (auto* irFunction = functionMap.tryGet(semaFunction)) {
        return irFunction;
    }
    if (semaFunction->isNative() || semaFunction->isGenerated()) {
        declQueue.push_back(semaFunction);
    }
    return declareFunction(semaFunction,
                           ctx,
                           mod,
                           typeMap,
                           functionMap,
                           config.nameMangler);
}

CallingConvention const& FuncGenContextBase::getCC(
    sema::Function const* function) {
    return functionMap.metaData(function).CC;
}

ir::ForeignFunction* FuncGenContextBase::getBuiltin(svm::Builtin builtin) {
    size_t index = static_cast<size_t>(builtin);
    auto* semaBuiltin = symbolTable.builtinFunction(index);
    auto* irBuiltin = getFunction(semaBuiltin);
    return cast<ir::ForeignFunction*>(irBuiltin);
}

ir::Call* FuncGenContextBase::callMemcpy(ir::Value* dest,
                                         ir::Value* source,
                                         ir::Value* numBytes) {
    auto* memcpy = getBuiltin(svm::Builtin::memcpy);
    std::array args = { dest, numBytes, source, numBytes };
    return add<ir::Call>(memcpy, args, std::string{});
}

ir::Call* FuncGenContextBase::callMemcpy(ir::Value* dest,
                                         ir::Value* source,
                                         size_t numBytes) {
    return callMemcpy(dest, source, ctx.intConstant(numBytes, 64));
}

ir::Call* FuncGenContextBase::callMemset(ir::Value* dest,
                                         ir::Value* numBytes,
                                         int value) {
    auto* memset = getBuiltin(svm::Builtin::memset);
    ir::Value* irVal = ctx.intConstant(utl::narrow_cast<uint64_t>(value), 64);
    std::array args = { dest, numBytes, irVal };
    return add<ir::Call>(memset, args, std::string{});
}

ir::Call* FuncGenContextBase::callMemset(ir::Value* dest,
                                         size_t numBytes,
                                         int value) {
    return callMemset(dest, ctx.intConstant(numBytes, 64), value);
}

static bool isDynArray(sema::ObjectType const* type) {
    auto* arr = dyncast<sema::ArrayType const*>(type);
    return arr && arr->isDynamic();
}

static bool isDynArrayPointer(sema::ObjectType const* type) {
    auto* ptr = dyncast<sema::PointerType const*>(type);
    return ptr && isDynArray(ptr->base().get());
}

ir::Value* FuncGenContextBase::toPackedRegister(Value const& value) {
    SC_ASSERT(!isDynArray(value.type()),
              "Cannot have dynamic array in register");
    if (value.isMemory()) {
        /// `{ Memory, Packed } -> { Register, Packed }`
        if (value.isPacked() || !isDynArrayPointer(value.type())) {
            return add<ir::Load>(value.get(0),
                                 typeMap.packed(value.type()),
                                 value.name());
        }
        /// `{ Memory, Unpacked } -> { Register, Packed }`
        else {
            auto* data = add<ir::Load>(value.get(0),
                                       ctx.ptrType(),
                                       utl::strcat(value.name(), ".data"));
            return packValues(ValueArray{ data, value.get(1) }, value.name());
        }
    }
    else {
        if (value.isPacked()) {
            /// `{ Register, Packed } -> { Register, Packed }`
            return value.get(0);
        }
        else {
            /// `{ Register, Unpacked } -> { Register, Packed }`
            return packValues(value.get(), value.name());
        }
    }
}

ir::Value* FuncGenContextBase::toPackedMemory(Value const& value) {
    if (isDynArray(value.type())) {
        if (value.isMemory()) {
            SC_ASSERT(value.isUnpacked(), "");
            return packValues(value.get(), value.name());
        }
        else {
            // Store to local memory here
            SC_UNIMPLEMENTED();
        }
    }
    else {
        if (value.isMemory()) {
            return value.get(0);
        }
        else {
            auto* packed = packValues(value.get(), value.name());
            return storeToMemory(packed, value.name());
        }
    }
}

utl::small_vector<ir::Value*, 2> FuncGenContextBase::toUnpackedRegister(
    Value const& value) {
    SC_ASSERT(!isDynArray(value.type()),
              "Cannot have dynamic array in register");
    if (value.isMemory()) {
        /// `{ Memory, {Packed,Unpacked} } -> { Register, Unpacked }`
        if (isDynArrayPointer(value.type())) {
            if (value.isPacked()) {
                auto* ptr =
                    add<ir::Load>(value.get(0), arrayPtrType, value.name());
                return unpackDynArrayPointerInRegister(ptr, value.name());
            }
            else {
                auto* ptr =
                    add<ir::Load>(value.get(0), ctx.ptrType(), value.name());
                return { ptr, value.get(1) };
            }
        }
        else {
            auto types = typeMap.unpacked(value.type());
            SC_ASSERT(types.size() == 1, "");
            return { add<ir::Load>(value.get(0), types.front(), value.name()) };
        }
    }
    else {
        if (value.isPacked()) {
            /// `{ Register, Packed } -> { Register, Unpacked }`
            if (isDynArrayPointer(value.type())) {
                return unpackDynArrayPointerInRegister(value.get(0),
                                                       value.name());
            }
            else {
                return { value.get(0) };
            }
        }
        else {
            /// `{ Register, Unpacked } -> { Register, Unpacked }`
            return value.get() | ToSmallVector<2>;
        }
    }
}

utl::small_vector<ir::Value*, 2> FuncGenContextBase::toUnpackedMemory(
    Value const& value) {
    if (value.isMemory()) {
        if (value.isPacked()) {
            if (isDynArray(value.type())) {
                /// This means `value.get(0)` is an array pointer in a register
                SC_EXPECT(value.get(0)->type() == arrayPtrType);
                return unpackDynArrayPointerInRegister(value.get(0),
                                                       value.name());
            }
            if (isDynArrayPointer(value.type())) {
                return unpackDynArrayPointerInMemory(value.get(0),
                                                     value.name());
            }
        }
        return value.get() | ToSmallVector<>;
    }
    else {
        if (value.isPacked() && isDynArrayPointer(value.type())) {
            return unpackDynArrayPointerInRegister(value.get(0), value.name());
        }
        return concat(single(storeToMemory(value.get(0))),
                      value.get().subspan(1)) |
               ToSmallVector<2>;
    }
}

Value FuncGenContextBase::getArraySize(sema::Type const* semaType,
                                       Value const& value) {
    auto name = utl::strcat(value.name(), ".count");
    if (auto* base = getPtrOrRefBase(semaType)) {
        semaType = base;
    }
    auto* arrType = dyncast<sema::ArrayType const*>(semaType);
    SC_ASSERT(arrType, "Need an array to get array size");
    auto* sizeType = symbolTable.Int();
    if (!arrType->isDynamic()) {
        return Value(name,
                     sizeType,
                     { ctx.intConstant(arrType->count(), 64) },
                     Register,
                     Unpacked);
    }
    if (value.location() == Memory) {
        if (value.representation() == Unpacked) {
            /// This case is weird, but for unpacked memory values the metadata
            /// values like array size are always in registers
            return Value(name, sizeType, { value.get(1) }, Register, Unpacked);
        }
        else if (isa<sema::PointerType>(semaType)) {
            auto* addr = add<ir::GetElementPointer>(ctx,
                                                    arrayPtrType,
                                                    value.get(0),
                                                    nullptr,
                                                    IndexArray{ 1 },
                                                    utl::strcat(name, ".addr"));
            return Value(name, sizeType, { addr }, Memory, Packed);
        }
        else {
            auto* size =
                add<ir::ExtractValue>(value.get(0), IndexArray{ 1 }, name);
            return Value(name, sizeType, { size }, Register, Packed);
        }
    }
    else {
        // Now value.location() == Register
        if (value.representation() == Unpacked) {
            return Value(name, sizeType, { value.get(1) }, Register, Unpacked);
        }
        else {
            auto* size =
                add<ir::ExtractValue>(value.get(0), IndexArray{ 1 }, name);
            return Value(name, sizeType, { size }, Register, Unpacked);
        }
    }
}

utl::small_vector<ir::Value*, 2> FuncGenContextBase::
    unpackDynArrayPointerInRegister(ir::Value* pointer, std::string name) {
    auto* data = add<ir::ExtractValue>(pointer,
                                       IndexArray{ 0 },
                                       utl::strcat(name, ".data"));
    auto* count = add<ir::ExtractValue>(pointer,
                                        IndexArray{ 1 },
                                        utl::strcat(name, ".count"));
    return { data, count };
}

utl::small_vector<ir::Value*, 2> FuncGenContextBase::
    unpackDynArrayPointerInMemory(ir::Value* ptr, std::string name) {
    auto* dataAddr =
        add<ir::GetElementPointer>(ctx,
                                   arrayPtrType,
                                   ptr,
                                   nullptr,
                                   IndexArray{ 0 },
                                   utl::strcat(name, ".data.addr"));
    auto* countAddr =
        add<ir::GetElementPointer>(ctx,
                                   arrayPtrType,
                                   ptr,
                                   nullptr,
                                   IndexArray{ 1 },
                                   utl::strcat(name, ".count.addr"));
    auto* count =
        add<ir::Load>(countAddr, ctx.intType(64), utl::strcat(name, ".count"));
    return { dataAddr, count };
}

ir::Value* FuncGenContextBase::makeCountToByteSize(ir::Value* count,
                                                   size_t elemSize) {
    return add<ir::ArithmeticInst>(count,
                                   ctx.intConstant(elemSize, 64),
                                   ir::ArithmeticOperation::Mul,
                                   "bytesize");
}

CountedForLoopDesc FuncGenContextBase::generateForLoop(std::string_view name,
                                                       ir::Value* tripCount) {
    auto* pred = &currentBlock();
    auto* body = newBlock(utl::strcat(name, ".body"));
    auto* end = newBlock(utl::strcat(name, ".end"));

    add<ir::Goto>(body);
    add(body);

    auto* phi =
        add<ir::Phi>(std::array{ ir::PhiMapping{ pred, ctx.intConstant(0, 64) },
                                 ir::PhiMapping{ body, nullptr } },
                     utl::strcat(name, ".counter"));
    auto* inc = add<ir::ArithmeticInst>(phi,
                                        ctx.intConstant(1, 64),
                                        ir::ArithmeticOperation::Add,
                                        utl::strcat(name, ".inc"));
    phi->setArgument(1, inc);
    auto* cond = add<ir::CompareInst>(inc,
                                      tripCount,
                                      ir::CompareMode::Unsigned,
                                      ir::CompareOperation::Equal,
                                      utl::strcat(name, ".test"));
    add<ir::Branch>(cond, end, body);
    add(end);

    return CountedForLoopDesc{ body, phi, ir::BasicBlock::ConstIterator(inc) };
}

CountedForLoopDesc FuncGenContextBase::generateForLoop(std::string_view name,
                                                       size_t tripCount) {
    return generateForLoop(name, ctx.intConstant(tripCount, 64));
}
