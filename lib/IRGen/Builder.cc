#include "IRGen/Builder.h"

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
    allocas.push_back(addr);
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

void FunctionBuilder::finish() {
    auto& entry = function.entry();
    auto before = entry.begin();
    for (auto* allocaInst: allocas) {
        entry.insert(before, allocaInst);
    }
    ir::setupInvariants(ctx, function);
}
