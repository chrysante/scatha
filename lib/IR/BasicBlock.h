#ifndef SCATHA_BASICBLOCK_H_
#define SCATHA_BASICBLOCK_H_

#include <string>

#include "IR/Context.h"
#include "IR/Instruction.h"
#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class Function;

class SCATHA(API) BasicBlock: public Value, public NodeWithParent<BasicBlock, Function> {
public:
    explicit BasicBlock(Context& context, std::string name):
        Value(NodeType::BasicBlock, context.voidType(), std::move(name)) {}

    void addInstruction(Instruction* instruction) {
        instruction->set_parent(this);
        instructions.push_back(instruction);
    }

    List<Instruction> instructions;
};

} // namespace scatha::ir

#endif // SCATHA_BASICBLOCK_H_
