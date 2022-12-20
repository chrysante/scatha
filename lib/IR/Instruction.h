#ifndef SCATHA_INSTRUCTION_H_
#define SCATHA_INSTRUCTION_H_

#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {
	
class BasicBlock;
class Type;

class Instruction: public Value, public NodeWithParent<Instruction, BasicBlock> {
protected:
    explicit Instruction(NodeType nodeType, Type const* type): Value(nodeType, type) {}
    
private:
    
};
	
} // namespace scatha::ir

#endif // SCATHA_INSTRUCTION_H_

