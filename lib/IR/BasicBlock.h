#ifndef SCATHA_BASICBLOCK_H_
#define SCATHA_BASICBLOCK_H_

#include <string>

#include "IR/Context.h"
#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class Function;
class Instruction;

class SCATHA(API) BasicBlock: public Value, public NodeWithParent<BasicBlock, Function> {
public:
    explicit BasicBlock(Context& context, std::string name);

    BasicBlock(BasicBlock const&) = delete;
    
    ~BasicBlock();

    void addInstruction(Instruction* instruction);

    List<Instruction> instructions;
};

} // namespace scatha::ir

#endif // SCATHA_BASICBLOCK_H_
