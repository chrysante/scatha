#include "BasicBlock.h"

#include "IR/Instruction.h"

using namespace scatha;
using namespace ir;

BasicBlock::BasicBlock(Context& context, std::string name):
    Value(NodeType::BasicBlock, std::move(name), context.voidType()) {}

BasicBlock::~BasicBlock() = default;

void BasicBlock::addInstruction(Instruction* instruction) {
    instruction->set_parent(this);
    instructions.push_back(instruction);
}

