#include "IR/CFG/Instruction.h"

#include "IR/CFG/BasicBlock.h"

using namespace scatha;
using namespace ir;

Instruction::Instruction(NodeType nodeType, Type const* type, std::string name,
                         std::span<Value* const> operands,
                         std::span<Type const* const> typeOperands):
    User(nodeType, type, std::move(name), operands),
    typeOps(typeOperands.begin(), typeOperands.end()) {}

void Instruction::setTypeOperand(size_t index, Type const* type) {
    SC_ASSERT(index < typeOps.size(), "Invalid index");
    typeOps[index] = type;
}

Function const* Instruction::parentFunction() const {
    return parent()->parent();
}

void BinaryInstruction::swapOperands() {
    std::swap(_operands[0], _operands[1]);
}
