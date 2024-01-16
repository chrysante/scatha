#include "IR/Builder.h"

#include <array>

#include <range/v3/view.hpp>

#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Common/Ranges.h"

using namespace scatha;
using namespace ir;
using namespace ranges::views;

BasicBlockBuilder::BasicBlockBuilder(Context& ctx, BasicBlock* BB):
    ctx(ctx), currentBB(BB), instAddPoint(BB->end()) {}

Instruction* BasicBlockBuilder::add(Instruction* inst) {
    return insert(instAddPoint.to_address(), inst);
}

Instruction* BasicBlockBuilder::insert(Instruction const* before,
                                       Instruction* inst) {
    currentBB->insert(before, inst);
    return inst;
}

Value* BasicBlockBuilder::buildStructure(StructType const* type,
                                       std::span<Value* const> members,
                                       std::string name) {
    SC_ASSERT(type->numElements() == members.size(), "Size mismatch");
    Value* value = ctx.undef(type);
    for (auto [index, member]: members | ranges::views::enumerate) {
        SC_ASSERT(member->type() == type->elementAt(index), "Type mismatch");
        value = add<InsertValue>(value, member, std::array{ index }, utl::strcat(name, ".elem.", index));
    }
    value->setName(name);
    return value;
}

Value* BasicBlockBuilder::packValues(std::span<Value* const> elems,
                                     std::string name) {
    switch (elems.size()) {
    case 0:
        SC_UNREACHABLE();
    
    case 1:
        return elems.front();
        
    default:
        return buildStructure(ctx.anonymousStruct(elems | transform(&Value::type) | ToSmallVector<>), 
                              elems, std::move(name));
    }
}

FunctionBuilder::FunctionBuilder(Context& ctx, Function* function):
    BasicBlockBuilder(ctx, nullptr), function(*function) {}

FunctionBuilder::~FunctionBuilder() = default;

BasicBlock* FunctionBuilder::newBlock(std::string name) {
    return new BasicBlock(ctx, std::move(name));
}

BasicBlock* FunctionBuilder::add(BasicBlock* BB) {
    function.pushBack(BB);
    currentBB = BB;
    instAddPoint = BB->end();
    return BB;
}

BasicBlock* FunctionBuilder::addNewBlock(std::string name) {
    return add(newBlock(std::move(name)));
}

Alloca* FunctionBuilder::makeLocalVariable(Type const* type, std::string name) {
    auto* addr = new Alloca(ctx, type, std::move(name));
    allocas.push_back(UniquePtr<Alloca>(addr));
    return addr;
}

Alloca* FunctionBuilder::makeLocalArray(Type const* type,
                                        Value* count,
                                        std::string name) {
    auto* addr = new Alloca(ctx, count, type, std::move(name));
    allocas.push_back(UniquePtr<Alloca>(addr));
    return addr;
}

Alloca* FunctionBuilder::storeToMemory(Value* value) {
    return storeToMemory(value, std::string(value->name()));
}

Alloca* FunctionBuilder::storeToMemory(Value* value, std::string name) {
    auto* addr = makeLocalVariable(value->type(), utl::strcat(name, ".addr"));
    add<Store>(addr, value);
    return addr;
}

void FunctionBuilder::insertAllocas() {
    auto& entry = function.entry();
    auto before = entry.begin();
    for (auto& allocaInst: allocas) {
        if (!allocaInst->unused()) {
            entry.insert(before, allocaInst.release());
        }
    }
}
