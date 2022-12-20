#ifndef SCATHA_BASICBLOCK_H_
#define SCATHA_BASICBLOCK_H_

#include <string>

#include "IR/Context.h"
#include "IR/Instruction.h"
#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class Function;
class Instruction;

class BasicBlock: public Value, public NodeWithParent<BasicBlock, Function> {
public:
    explicit BasicBlock(Context& context, std::string name) noexcept:
        Value(NodeType::BasicBlock, std::move(name), context.voidType()) {}

    ~BasicBlock();

    void addInstruction(Instruction* instruction);

private:
    List<Instruction> instructions;
};

} // namespace scatha::ir

#endif // SCATHA_BASICBLOCK_H_
