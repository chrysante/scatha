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
    return declareFunction(semaFunction, ctx, mod, typeMap, functionMap,
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

ir::Call* FuncGenContextBase::callMemcpy(ir::Value* dest, ir::Value* source,
                                         ir::Value* numBytes) {
    auto* memcpy = getBuiltin(svm::Builtin::memcpy);
    std::array args = { dest, numBytes, source, numBytes };
    return add<ir::Call>(memcpy, args, std::string{});
}

ir::Call* FuncGenContextBase::callMemcpy(ir::Value* dest, ir::Value* source,
                                         size_t numBytes) {
    return callMemcpy(dest, source, ctx.intConstant(numBytes, 64));
}

ir::Call* FuncGenContextBase::callMemset(ir::Value* dest, ir::Value* numBytes,
                                         int value) {
    auto* memset = getBuiltin(svm::Builtin::memset);
    ir::Value* irVal = ctx.intConstant(utl::narrow_cast<uint64_t>(value), 64);
    std::array args = { dest, numBytes, irVal };
    return add<ir::Call>(memset, args, std::string{});
}

ir::Call* FuncGenContextBase::callMemset(ir::Value* dest, size_t numBytes,
                                         int value) {
    return callMemset(dest, ctx.intConstant(numBytes, 64), value);
}

Value FuncGenContextBase::to(ValueRepresentation repr, Value const& value) {
    switch (repr) {
    case Packed:
        return pack(value);
    case Unpacked:
        return unpack(value);
    default:
        SC_UNREACHABLE();
    }
}

Value FuncGenContextBase::pack(Value const& value) {
    if (value.isPacked()) {
        return value;
    }
    auto atom = [&]() -> Atom {
        if (isDynArray(value.type())) {
            SC_ASSERT(value[0].isMemory(), "Dyn array must be in memory");
            SC_ASSERT(value[0]->type() == ctx.ptrType(),
                      "Reference to dyn array must be arrayPtrType");
            auto* packed =
                packValues(ValueArray{ value[0].get(),
                                       toRegister(value[1], ctx.intType(64),
                                                  utl::strcat(value.name(),
                                                              ".count"))
                                           .get() },
                           value.name());
            return Atom(packed, Memory);
        }
        else if (isDynArrayPointer(value.type())) {
            auto types = typeMap.unpacked(value.type());
            auto elems = zip(value, types) | transform([&](auto p) {
                auto [atom, type] = p;
                return toRegister(atom, type, value.name()).get();
            }) | ToSmallVector<2>;
            auto* packed = packValues(elems, value.name());
            return Atom(packed, Register);
        }
        else {
            return { value.single() };
        }
    }();
    return Value::Packed(value.name(), value.type(), atom);
}

Value FuncGenContextBase::unpack(Value const& value) {
    if (value.isUnpacked()) {
        return value;
    }
    auto atoms = [&]() -> utl::small_vector<Atom, 2> {
        if (isDynArray(value.type())) {
            SC_ASSERT(value.single().isMemory(), "Dyn array must be in memory");
            SC_ASSERT(value.single()->type() == arrayPtrType,
                      "Reference to dyn array must be arrayPtrType");
            auto atoms = unpackRegister(Atom::Register(value.single().get()),
                                        value.name());
            atoms[0] = Atom(atoms[0].get(), Memory);
            return atoms;
        }
        else if (isDynArrayPointer(value.type())) {
            auto atom = value.single();
            switch (atom.location()) {
            case Register: {
                return unpackRegister(atom, value.name());
            }
            case Memory: {
                return unpackMemory(atom, arrayPtrType, value.name());
            }
            default:
                SC_UNREACHABLE();
            }
        }
        else {
            return { value.single() };
        }
    }();
    return Value::Unpacked(value.name(), value.type(), atoms);
}

Atom FuncGenContextBase::to(ValueLocation location, Atom atom,
                            ir::Type const* type, std::string name) {
    switch (location) {
    case Register:
        return toRegister(atom, type, std::move(name));
    case Memory:
        return toMemory(atom);
    default:
        SC_UNREACHABLE();
    }
}

Atom FuncGenContextBase::toMemory(Atom atom) {
    if (atom.isMemory()) {
        return atom;
    }
    return Atom::Memory(storeToMemory(atom.get()));
}

Atom FuncGenContextBase::toRegister(Atom atom, ir::Type const* type,
                                    std::string name) {
    if (atom.isRegister()) {
        return atom;
    }
    auto* load = add<ir::Load>(atom.get(), type, name);
    return Atom::Register(load);
}

utl::small_vector<Atom, 2> FuncGenContextBase::unpackRegister(
    Atom atom, std::string name) {
    SC_EXPECT(atom.isRegister());
    auto* type = dyncast<ir::RecordType const*>(atom->type());
    if (!type) {
        return { atom };
    }
    return iota(size_t{ 0 }, type->numElements()) |
           transform([&](size_t index) {
        auto* elem = add<ir::ExtractValue>(atom.get(), IndexArray{ index },
                                           utl::strcat(name, ".elem.", index));
        return Atom(elem, Register);
    }) | ToSmallVector<2>;
}

utl::small_vector<Atom, 2> FuncGenContextBase::unpackMemory(
    Atom atom, ir::RecordType const* type, std::string name) {
    SC_EXPECT(atom.isMemory());
    return type->elements() | enumerate | transform([&](auto p) {
        auto [index, memType] = p;
        auto* elem = add<ir::GetElementPointer>(type, atom.get(), nullptr,
                                                IndexArray{ size_t(index) },
                                                utl::strcat(name, ".elem.",
                                                            index, ".addr"));
        return Atom(elem, Memory);
    }) | ToSmallVector<2>;
}

ir::Value* FuncGenContextBase::toPackedRegister(Value const& value) {
    auto* type = typeMap.packed(value.type());
    return toRegister(pack(value).single(), type, value.name()).get();
}

ir::Value* FuncGenContextBase::toPackedMemory(Value const& value) {
    return toMemory(pack(value).single()).get();
}

Value FuncGenContextBase::getArraySize(sema::Type const* semaType,
                                       Value const& value) {
    auto name = utl::strcat(value.name(), ".count");
    if (auto* base = getPtrOrRefBase(semaType)) {
        semaType = base;
    }
    auto* arrType = cast<sema::ArrayType const*>(semaType);
    auto* sizeType = symbolTable.Int();
    if (!arrType->isDynamic()) {
        return Value::Packed(name, sizeType,
                             Atom::Register(
                                 ctx.intConstant(arrType->count(), 64)));
    }
    if (value.isUnpacked()) {
        return Value::Packed(name, sizeType, value[1]);
    }
    else if (value[0].isMemory()) {
        if (isa<sema::PointerType>(semaType)) {
            auto* addr = add<ir::GetElementPointer>(ctx, arrayPtrType,
                                                    value[0].get(), nullptr,
                                                    IndexArray{ 1 },
                                                    utl::strcat(name, ".addr"));
            return Value::Packed(name, sizeType, Atom::Memory(addr));
        }
        else {
            auto* size =
                add<ir::ExtractValue>(value[0].get(), IndexArray{ 1 }, name);
            return Value::Packed(name, sizeType, Atom::Register(size));
        }
    }

    else {
        // Now value[0].isRegister()
        auto* size =
            add<ir::ExtractValue>(value[0].get(), IndexArray{ 1 }, name);
        return Value::Packed(name, sizeType, Atom::Register(size));
    }
}

ir::Value* FuncGenContextBase::makeCountToByteSize(ir::Value* count,
                                                   size_t elemSize) {
    if (auto* constant = dyncast<ir::IntegralConstant*>(count)) {
        auto count = constant->value();
        auto bytesize = mul(count, APInt(elemSize, count.bitwidth()));
        return ctx.intConstant(bytesize);
    }
    return add<ir::ArithmeticInst>(count, ctx.intConstant(elemSize, 64),
                                   ir::ArithmeticOperation::Mul, "bytesize");
}

Value FuncGenContextBase::copyValue(Value const& value) {
    SC_EXPECT(value.type()->hasTrivialLifetime() ||
              isa<sema::UniquePtrType>(value.type()));
    auto repr = value.representation();
    if (value.type()->size() <= PreferredMaxRegisterValueSize) {
        auto types = typeMap.map(repr, value.type());
        SC_ASSERT(types.size() == value.size(), "");
        auto elems = zip(value, types) | transform([&](auto p) {
            auto [atom, type] = p;
            return toRegister(atom, type, value.name());
        }) | ToSmallVector<2>;
        return Value(value.name(), value.type(), elems, repr);
    }
    else {
        auto* irType = typeMap.packed(value.type());
        auto* mem = makeLocalVariable(irType, value.name());
        callMemcpy(mem, toMemory(pack(value).single()).get(), irType->size());
        return Value::Packed(value.name(), value.type(), Atom::Memory(mem));
    }
}

void FuncGenContextBase::generateForLoop(
    std::string_view name, std::span<ir::Value* const> countersBegin,
    ir::Value* counterEnd,
    utl::function_view<
        utl::small_vector<ir::Value*>(std::span<ir::Value* const> counters)>
        inc,
    utl::function_view<void(std::span<ir::Value* const> counters)> genBody) {
    auto* pred = &currentBlock();
    auto* body = newBlock(utl::strcat(name, ".body"));
    auto* end = newBlock(utl::strcat(name, ".end"));
    add<ir::Goto>(body);
    add(body);
    auto counters = countersBegin |
                    transform([&](ir::Value* begin) -> ir::Value* {
        return add<ir::Phi>(std::array{ ir::PhiMapping{ pred, begin },
                                        ir::PhiMapping{ body, nullptr } },
                            utl::strcat(name, ".counter"));
    }) | ToSmallVector<>;
    genBody(counters);
    auto increments = inc(counters);
    for (auto [counter, increment]:
         zip(counters | transform(cast<ir::Phi*>), increments))
    {
        counter->setArgument(1, increment);
        counter->setPredecessor(1, &currentBlock());
    }
    auto* cond = add<ir::CompareInst>(increments[0], counterEnd,
                                      ir::CompareMode::Unsigned,
                                      ir::CompareOperation::Equal,
                                      utl::strcat(name, ".test"));
    add<ir::Branch>(cond, end, body);
    add(end);
}

Value FuncGenContextBase::makeVoidValue(std::string name) const {
    return Value::Packed(std::move(name), symbolTable.Void(),
                         Atom::Register(ctx.voidValue()));
}
