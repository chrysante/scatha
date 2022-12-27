#ifndef SCATHA_INSTRUCTION_H_
#define SCATHA_INSTRUCTION_H_

#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class BasicBlock;
class Type;

class SCATHA(API) Instruction: public Value, public NodeWithParent<Instruction, BasicBlock> {
protected:
    explicit Instruction(NodeType nodeType, Type const* type, std::string name = {}):
        Value(nodeType, type, std::move(name)) {}

private:
};

} // namespace scatha::ir

#endif // SCATHA_INSTRUCTION_H_
