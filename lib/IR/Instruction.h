#ifndef SCATHA_INSTRUCTION_H_
#define SCATHA_INSTRUCTION_H_

#include "IR/Context.h"
#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class BasicBlock;
class Type;

class Instruction: public Value, public NodeWithParent<Instruction, BasicBlock> {
protected:
    explicit Instruction(NodeType nodeType, Type const* type, std::string name = {}):
        Value(nodeType, type, std::move(name)) {}
    
private:
    
};

class UnaryInstruction: public Instruction {
protected:
    explicit UnaryInstruction(NodeType nodeType, Value* operand, Type const* type):
        Instruction(nodeType, type), _operand(operand) {}
    
public:
    Value* operand() { return _operand; }
    
private:
    Value* _operand;
};

class Alloca: public UnaryInstruction {
public:
    explicit Alloca(Context& context, Value* sizeOp):
        UnaryInstruction(NodeType::Alloca, sizeOp, context.voidType())
    {
        SC_ASSERT(sizeOp->type()->isIntegral(), "Size argument to Alloca must be integral");
    }
    
private:
    
};

enum class CompareOperation {
    Less,
    LessEq,
    Greater,
    GreaterEq,
    Equal,
    NotEqual,
};

class SCATHA(API) CompareInst: public Instruction {
public:
    explicit CompareInst(Context& context, Value* lhs, Value* rhs, CompareOperation op):
        Instruction(NodeType::CompareInst, context.integralType(1)),
        _lhs(lhs),
        _rhs(rhs),
        _op(op)
    {}
    
    Value* lhs() const { return _lhs; }
    Value* rhs() const { return _rhs; }
    CompareOperation operation() const { return _op; }
    
private:
    Value* _lhs;
    Value* _rhs;
    CompareOperation _op;
};

class TerminatorInst: public Instruction {
protected:
    explicit TerminatorInst(NodeType nodeType, Context& context): Instruction(nodeType, context.voidType()) {}
};

class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target): TerminatorInst(NodeType::Goto, context), _target(target) {}

    BasicBlock* target() const { return _target; }

private:
    BasicBlock* _target;
};

class Branch: public TerminatorInst {
public:
    explicit Branch(Context& context, Value* condition, BasicBlock* ifTarget, BasicBlock* elseTarget):
        TerminatorInst(NodeType::Branch, context), _condition(condition), _if(ifTarget), _else(elseTarget) {
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
    explicit Return(Context& context, Value* value): TerminatorInst(NodeType::Return, context), _value(value) {}

    Value* value() { return _value; }

private:
    Value* _value;
};

} // namespace scatha::ir

#endif // SCATHA_INSTRUCTION_H_
