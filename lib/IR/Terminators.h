#ifndef SCATHA_IR_BRANCH_H_
#define SCATHA_IR_BRANCH_H_

#include <utl/utility.hpp>

#include "Basic/Basic.h"
#include "IR/Instruction.h"
#include "IR/Context.h"

namespace scatha::ir {

class BasicBlock;

class Goto: public Instruction {
public:
    explicit Goto(Context& context, BasicBlock* target): Instruction(context.voidType()), _target(target) {}
    
    BasicBlock* target() const { return _target; }
    
private:
    BasicBlock* _target;
};

class Branch: public Instruction {
public:
    explicit Branch(Context& context, Value* condition, BasicBlock* ifTarget, BasicBlock* elseTarget):
        Instruction(context.voidType()), _condition(condition), _if(ifTarget), _else(elseTarget)
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

class Return: public Instruction {
public:
    explicit Return(Context& context, Value* value): Instruction(context.voidType()), _value(value) {}
    
    Value* value() { return _value; }
    
private:
    Value* _value;
};

//class BranchStatement: public StatementBase {
//public:
//    
//private:
//    
//};
	
} // namespace scatha::ir

#endif // SCATHA_IR_BRANCH_H_

