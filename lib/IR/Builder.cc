#include "IR/Builder.h"

#include <array>

#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

BasicBlockBuilder::BasicBlockBuilder(Context& ctx, BasicBlock* BB):
    ctx(ctx), currentBB(BB) {}

Instruction* BasicBlockBuilder::add(Instruction* inst) {
    currentBB->pushBack(inst);
    return inst;
}

Instruction* BasicBlockBuilder::insert(Instruction const* before,
                                       Instruction* inst) {
    currentBB->insert(before, inst);
    return inst;
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
    return storeToMemory(value, utl::strcat(value->name(), ".addr"));
}

Alloca* FunctionBuilder::storeToMemory(Value* value, std::string name) {
    auto* addr = makeLocalVariable(value->type(), std::move(name));
    add<Store>(addr, value);
    return addr;
}

Value* FunctionBuilder::buildStructure(StructType const* type,
                                       std::span<Value* const> members,
                                       std::string name) {
    SC_ASSERT(type->numElements() == members.size(), "Size mismatch");
    Value* value = ctx.undef(type);
    for (auto [index, member]: members | ranges::views::enumerate) {
        SC_ASSERT(member->type() == type->elementAt(index), "Type mismatch");
        value = add<InsertValue>(value, member, std::array{ index }, name);
    }
    return value;
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
