#include "BasicBlock.h"

#include "IR/Instruction.h"

using namespace scatha;
using namespace ir;

void BasicBlock::addInstruction(Instruction* instruction) {
    instruction->set_parent(this);
    instructions.push_back(instruction);
}

BasicBlock::~BasicBlock() = default;
