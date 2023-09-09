#include "IRGen/Builder.h"

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace irgen;

Builder::Builder(ir::Context& ctx, ir::Function& function):
    ctx(ctx), function(function) {}

ir::BasicBlock* Builder::newBlock(std::string name) {
    return new ir::BasicBlock(ctx, std::move(name));
}

ir::BasicBlock* Builder::add(ir::BasicBlock* BB) {
    function.pushBack(BB);
    currentBB = BB;
    return BB;
}

ir::BasicBlock* Builder::addNewBlock(std::string name) {
    return add(newBlock(std::move(name)));
}

ir::Instruction* Builder::add(ir::Instruction* inst) {
    currentBlock().pushBack(inst);
    return inst;
}

ir::Value* Builder::makeLocalVariable(ir::Type const* type, std::string name) {
    auto* addr = new ir::Alloca(ctx, type, std::move(name));
    allocas.push_back(addr);
    return addr;
}

ir::Value* Builder::storeToMemory(ir::Value* value) {
    return storeToMemory(value, utl::strcat(value->name(), ".addr"));
}

ir::Value* Builder::storeToMemory(ir::Value* value, std::string name) {
    auto* addr = makeLocalVariable(value->type(), std::move(name));
    add<ir::Store>(addr, value);
    return addr;
}
