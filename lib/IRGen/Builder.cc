#include "IRGen/Builder.h"

#include <array>

#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace irgen;

BasicBlockBuilder::BasicBlockBuilder(ir::Context& ctx, ir::BasicBlock* BB):
    ctx(ctx), currentBB(BB) {}

ir::Instruction* BasicBlockBuilder::add(ir::Instruction* inst) {
    currentBB->pushBack(inst);
    return inst;
}

FunctionBuilder::FunctionBuilder(ir::Context& ctx, ir::Function* function):
    BasicBlockBuilder(ctx, nullptr), function(*function) {}

FunctionBuilder::~FunctionBuilder() = default;

ir::BasicBlock* FunctionBuilder::newBlock(std::string name) {
    return new ir::BasicBlock(ctx, std::move(name));
}

ir::BasicBlock* FunctionBuilder::add(ir::BasicBlock* BB) {
    function.pushBack(BB);
    currentBB = BB;
    return BB;
}

ir::BasicBlock* FunctionBuilder::addNewBlock(std::string name) {
    return add(newBlock(std::move(name)));
}

ir::Alloca* FunctionBuilder::makeLocalVariable(ir::Type const* type,
                                               std::string name) {
    auto* addr = new ir::Alloca(ctx, type, std::move(name));
    allocas.push_back(UniquePtr<ir::Alloca>(addr));
    return addr;
}

ir::Alloca* FunctionBuilder::storeToMemory(ir::Value* value) {
    return storeToMemory(value, utl::strcat(value->name(), ".addr"));
}

ir::Alloca* FunctionBuilder::storeToMemory(ir::Value* value, std::string name) {
    auto* addr = makeLocalVariable(value->type(), std::move(name));
    add<ir::Store>(addr, value);
    return addr;
}

ir::Value* FunctionBuilder::buildStructure(ir::StructType const* type,
                                           std::span<ir::Value* const> members,
                                           std::string name) {
    SC_ASSERT(type->numElements() == members.size(), "Size mismatch");
    ir::Value* value = ctx.undef(type);
    for (auto [index, member]: members | ranges::views::enumerate) {
        SC_ASSERT(member->type() == type->elementAt(index), "Type mismatch");
        value = add<ir::InsertValue>(value, member, std::array{ index }, name);
    }
    return value;
}

void FunctionBuilder::finish() {
    auto& entry = function.entry();
    auto before = entry.begin();
    for (auto& allocaInst: allocas) {
        if (allocaInst->isUsed()) {
            entry.insert(before, allocaInst.release());
        }
    }
    ir::setupInvariants(ctx, function);
}
