#include "IRGen/LoweringContext.h"

#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;

using enum ValueLocation;

void LoweringContext::emitDestructorCalls(sema::DTorStack const& dtorStack) {
    for (auto call: dtorStack) {
        auto* function = getFunction(call.destructor);
        auto* object = objectMap[call.object].get();
        SC_ASSERT(object, "");
        add<ir::Call>(function,
                      std::array<ir::Value*, 1>{ object },
                      std::string{});
    }
}

ir::BasicBlock* LoweringContext::newBlock(std::string name) {
    return new ir::BasicBlock(ctx, std::move(name));
}

void LoweringContext::add(ir::BasicBlock* block) {
    currentFunction->pushBack(block);
    currentBlock = block;
}

ir::BasicBlock* LoweringContext::addNewBlock(std::string name) {
    auto* block = newBlock(std::move(name));
    add(block);
    return block;
}

void LoweringContext::add(ir::Instruction* inst) {
    currentBlock->pushBack(inst);
}

ir::Value* LoweringContext::toRegister(Value value) {
    switch (value.location()) {
    case Register:
        return value.get();
    case Memory:
        return add<ir::Load>(value.get(),
                             value.type(),
                             std::string(value.get()->name()));
    }
}

ir::Value* LoweringContext::toMemory(Value value) {
    switch (value.location()) {
    case Register:
        return storeLocal(value.get());

    case Memory:
        return value.get();
    }
}

ir::Value* LoweringContext::toValueLocation(ValueLocation location,
                                            Value value) {
    switch (location) {
    case Register:
        return toRegister(value);
    case Memory:
        return toMemory(value);
    }
}

ir::Alloca* LoweringContext::storeLocal(ir::Value* value, std::string name) {
    if (name.empty()) {
        name = utl::strcat(value->name(), ".addr");
    }
    auto* addr = makeLocal(value->type(), std::move(name));
    add<ir::Store>(addr, value);
    return addr;
}

ir::Alloca* LoweringContext::makeLocal(ir::Type const* type, std::string name) {
    auto* addr = new ir::Alloca(ctx, type, std::move(name));
    allocas.push_back(addr);
    return addr;
}

ir::Callable* LoweringContext::getFunction(sema::Function const* function) {
    switch (function->kind()) {
    case sema::FunctionKind::Native:
        return functionMap.find(function)->second;

    case sema::FunctionKind::External:
        [[fallthrough]];
    case sema::FunctionKind::Generated: {
        auto itr = functionMap.find(function);
        if (itr != functionMap.end()) {
            return itr->second;
        }
        return declareFunction(function);
    }
    }
}

void LoweringContext::memorizeObject(sema::Object const* object, Value value) {
    bool success = objectMap.insert({ object, value }).second;
    SC_ASSERT(success, "Redeclaration");
}

Value LoweringContext::getObject(sema::Object const* object) const {
    auto itr = objectMap.find(object);
    SC_ASSERT(itr != objectMap.end(), "Not found");
    return itr->second;
}

void LoweringContext::memorizeArraySize(sema::Object const* object,
                                        Value size) {
    memorizeLazyArraySize(object, [size](ir::BasicBlock*) { return size; });
}

void LoweringContext::memorizeArraySize(sema::Object const* object,
                                        size_t count) {
    memorizeArraySize(object, Value(ctx.intConstant(count, 64), Register));
}

void LoweringContext::memorizeLazyArraySize(
    sema::Object const* object, std::function<Value(ir::BasicBlock*)> getter) {
    SC_ASSERT(object, "Must not be null");
    auto [itr, success] = arraySizeMap.insert({ object, std::move(getter) });
    SC_ASSERT(success, "ID already present");
}

void LoweringContext::memorizeArraySizeOf(sema::Object const* newObj,
                                          sema::Object const* original) {
    memorizeArraySize(newObj, getArraySize(original));
}

Value LoweringContext::getArraySize(sema::Object const* object) const {
    SC_ASSERT(object, "");
    auto result = tryGetArraySize(object);
    SC_ASSERT(result, "Not found");
    return *result;
}

std::optional<Value> LoweringContext::tryGetArraySize(
    sema::Object const* object) const {
    auto itr = arraySizeMap.find(object);
    if (itr == arraySizeMap.end()) {
        return std::nullopt;
    }
    return itr->second(currentBlock);
}
