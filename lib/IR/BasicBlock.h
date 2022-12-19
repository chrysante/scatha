#ifndef SCATHA_BASICBLOCK_H_
#define SCATHA_BASICBLOCK_H_

#include <string>

#include <utl/vector.hpp>

#include "IR/Value.h"
#include "IR/Instruction.h"
#include "IR/Context.h"

namespace scatha::ir {

class BasicBlock: public Value {
public:
    explicit BasicBlock(Context& context, std::string name):
        Value(std::move(name), context.voidType()) {}
    
    void addInstruction(Instruction* instruction) {
        instructions.push_back(instruction);
    }
    
private:
    utl::small_vector<Instruction*> instructions;
};
	
} // namespace scatha::ir

#endif // SCATHA_BASICBLOCK_H_

