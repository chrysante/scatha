#ifndef SCATHA_IR_INSTRUCTIONS_H_
#define SCATHA_IR_INSTRUCTIONS_H_

#include <string>
#include <span>

#include <utl/vector.hpp>

#include "IR/Context.h"
#include "IR/Instruction.h"

namespace scatha::ir {

class SCATHA(API) Alloca: public Instruction {
public:
    explicit Alloca(Context& context, Type const* allocatedType, std::string name):
        Instruction(NodeType::Alloca, context.pointerType(), std::move(name)), _allocatedType(allocatedType) {}

    Type const* allocatedType() const { return _allocatedType; }

private:
    Type const* _allocatedType;
};

class SCATHA(API) UnaryInstruction: public Instruction {
protected:
    explicit UnaryInstruction(NodeType nodeType, Value* operand, Type const* type, std::string name):
        Instruction(nodeType, type, std::move(name)), _operand(operand) {}

public:
    Value* operand() { return _operand; }
    Value const* operand() const { return _operand; }

private:
    Value* _operand;
};

class SCATHA(API) Load: public UnaryInstruction {
public:
    explicit Load(Type const* type, Value* address, std::string name):
        UnaryInstruction(NodeType::Load, address, type, std::move(name)) {
        SC_ASSERT(address->type()->isPointer(), "Address argument to Load must be a pointer");
    }

private:
};

class SCATHA(API) BinaryInstruction: public Instruction {
protected:
    explicit BinaryInstruction(NodeType nodeType, Value* lhs, Value* rhs, Type const* type, std::string name = {}):
        Instruction(nodeType, type, std::move(name)), _lhs(lhs), _rhs(rhs) {}

public:
    Value* lhs() { return _lhs; }
    Value const* lhs() const { return _lhs; }
    Value* rhs() { return _rhs; }
    Value const* rhs() const { return _rhs; }

private:
    Value* _lhs;
    Value* _rhs;
};

class SCATHA(API) Store: public BinaryInstruction {
public:
    explicit Store(Context& context, Value* address, Value* value):
        BinaryInstruction(NodeType::Store, address, value, context.voidType()) {
        SC_ASSERT(address->type()->isPointer(), "Address argument to Store must be a pointer");
    }

private:
};

class SCATHA(API) CompareInst: public BinaryInstruction {
public:
    explicit CompareInst(Context& context, Value* lhs, Value* rhs, CompareOperation op, std::string name):
        BinaryInstruction(NodeType::CompareInst, lhs, rhs, context.integralType(1), std::move(name)), _op(op) {}

    CompareOperation operation() const { return _op; }

private:
    CompareOperation _op;
};

class SCATHA(API) ArithmeticInst: public BinaryInstruction {
public:
    explicit ArithmeticInst(Value* lhs, Value* rhs, ArithmeticOperation op, std::string name):
        BinaryInstruction(NodeType::ArithmeticInst, lhs, rhs, lhs->type(), std::move(name)), _op(op) {
        SC_ASSERT(lhs->type() == rhs->type(), "Operands must have the same type");
    }

    ArithmeticOperation operation() const { return _op; }

private:
    ArithmeticOperation _op;
};

class SCATHA(API) TerminatorInst: public Instruction {
protected:
    explicit TerminatorInst(NodeType nodeType, Context& context): Instruction(nodeType, context.voidType()) {}
};

class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target): TerminatorInst(NodeType::Goto, context), _target(target) {}

    BasicBlock* target() { return _target; }
    BasicBlock const* target() const { return _target; }

private:
    BasicBlock* _target;
};

class SCATHA(API) Branch: public TerminatorInst {
public:
    explicit Branch(Context& context, Value* condition, BasicBlock* ifTarget, BasicBlock* elseTarget):
        TerminatorInst(NodeType::Branch, context), _condition(condition), _if(ifTarget), _else(elseTarget) {
        SC_ASSERT(condition->type()->category() == Type::Category::Integral &&
                      static_cast<Integral const*>(condition->type())->bitWidth() == 1,
                  "Condition must be of type i1");
    }

    Value* condition() { return _condition; }
    Value const* condition() const { return _condition; }
    BasicBlock* ifTarget() { return _if; }
    BasicBlock const* ifTarget() const { return _if; }
    BasicBlock* elseTarget() { return _else; }
    BasicBlock const* elseTarget() const { return _else; }

private:
    Value* _condition;
    BasicBlock* _if;
    BasicBlock* _else;
};

class SCATHA(API) Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value): TerminatorInst(NodeType::Return, context), _value(value) {}

    Value* value() { return _value; }
    Value const* value() const { return _value; }

private:
    Value* _value;
};

class SCATHA(API) FunctionCall: public Instruction {
public:
    FunctionCall(Function* function, std::span<Value* const> arguments, std::string name = {});

    Function* function() { return _function; }
    Function const* function() const { return _function; }

    std::span<Value* const> arguments() { return _args; }
    std::span<Value const* const> arguments() const { return _args; }

private:
    Function* _function;
    utl::small_vector<Value*> _args;
};

struct PhiMapping {
    BasicBlock* pred;
    Value* value;
};

class SCATHA(API) Phi: public Instruction {
public:
    explicit Phi(Type const* type, std::initializer_list<PhiMapping> args, std::string name):
        Phi(type, std::span<PhiMapping const>(args), std::move(name)) {}
    explicit Phi(Type const* type, std::span<PhiMapping const> args, std::string name):
        Instruction(NodeType::Phi, type, std::move(name)), arguments(args) {
        for ([[maybe_unused]] auto& [pred, val]: args) {
            SC_EXPECT(val->type() == type, "Type mismatch");
        }
    }

    utl::small_vector<PhiMapping> arguments;
};


} // namespace scatha::ir

#endif // SCATHA_IR_INSTRUCTIONS_H_

