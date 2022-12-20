#ifndef SCATHA_INSTRUCTION_H_
#define SCATHA_INSTRUCTION_H_

#include "IR/List.h"
#include "IR/Value.h"
#include "IR/Context.h"

namespace scatha::ir {
	
class BasicBlock;
class Type;

class Instruction: public Value, public NodeWithParent<Instruction, BasicBlock> {
protected:
    explicit Instruction(NodeType nodeType, Type const* type): Value(nodeType, type) {}
    
private:
    
};

class TerminatorInst: public Instruction {
protected:
    explicit TerminatorInst(NodeType nodeType, Context& context):
    Instruction(nodeType, context.voidType())
    {}
};

class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target):
    TerminatorInst(NodeType::Goto, context), _target(target) {}
    
    BasicBlock* target() const { return _target; }
    
private:
    BasicBlock* _target;
};

class Branch: public TerminatorInst {
public:
    explicit Branch(Context& context, Value* condition, BasicBlock* ifTarget, BasicBlock* elseTarget):
    TerminatorInst(NodeType::Branch, context), _condition(condition), _if(ifTarget), _else(elseTarget)
    {
        SC_ASSERT(condition->type()->category() == Type::Category::Integral &&
                  static_cast<Integral const*>(condition->type())->bitWidth() == 1,
                  "Condition must be of type i1");
    }
    
    Value* condition() { return _condition; }
    BasicBlock* ifTarget() { return _if; }
    BasicBlock* elseTarget() { return _else; }
    
private:
    Value* _condition;
    BasicBlock* _if;
    BasicBlock* _else;
};

class Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value):
    TerminatorInst(NodeType::Return, context), _value(value) {}
    
    Value* value() { return _value; }
    
private:
    Value* _value;
};
	
} // namespace scatha::ir

#endif // SCATHA_INSTRUCTION_H_

