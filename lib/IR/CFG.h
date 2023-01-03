#ifndef SCATHA_IR_CFG_H_
#define SCATHA_IR_CFG_H_

#include <string>

#include <utl/vector.hpp>

#include "Common/APInt.h"
#include "IR/CFGCommon.h"
#include "IR/Type.h"
#include "IR/List.h"

namespace scatha::ir {

class Context;
class Module;

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class SCATHA(API) Value {
protected:
    explicit Value(NodeType nodeType, Type const* type): _nodeType(nodeType), _type(type) {}

    explicit Value(NodeType nodeType, Type const* type, std::string name) noexcept:
        _nodeType(nodeType), _type(type), _name(std::move(name)) {}

public:
    NodeType nodeType() const { return _nodeType; }

    Type const* type() const { return _type; }

    std::string_view name() const { return _name; }

    bool hasName() const { return !_name.empty(); }

    void setName(std::string name) { _name = std::move(name); }

private:
    NodeType _nodeType;
    Type const* _type;
    std::string _name;
};

/// For dyncast compatibilty of the CFG
inline NodeType dyncast_get_type(std::derived_from<Value> auto const& value) {
    return value.nodeType();
}

/// Represents a (global) constant value.
class SCATHA(API) Constant: public Value {
public:
    using Value::Value;

private:
};

/// Represents a global integral constant value.
class SCATHA(API) IntegralConstant: public Constant {
public:
    explicit IntegralConstant(Context& context, APInt value, size_t bitWidth);

    APInt const& value() const { return _value; }

private:
    APInt _value;
};

/// Base class of all instructions. Every instruction inherits from \p Value as it (usually) yields a value. If an
/// instruction does not yield a value its \p Value super class is of type void.
class SCATHA(API) Instruction: public Value, public NodeWithParent<Instruction, BasicBlock> {
protected:
    explicit Instruction(NodeType nodeType, Type const* type, std::string name = {}):
        Value(nodeType, type, std::move(name)) {}
};

/// Represents a basic block. A basic block starts is a list of instructions starting with zero or more phi nodes and
/// ending with one terminator instruction. These invariants are not enforced by this class because they may be
/// violated during construction and transformations of the CFG.
class SCATHA(API) BasicBlock: public Value, public NodeWithParent<BasicBlock, Function> {
public:
    explicit BasicBlock(Context& context, std::string name);

    void addInstruction(Instruction* instruction) {
        instruction->set_parent(this);
        instructions.push_back(instruction);
    }

    /// Check wether this is the entry basic block of a function
    bool isEntry() const;
    
    List<Instruction> instructions;
};

/// Represents a function parameter.
class Parameter: public Value, public NodeWithParent<Parameter, Function> {
    using NodeBase = NodeWithParent<Parameter, Function>;

public:
    explicit Parameter(Type const* type, std::string name, Function* parent):
        Value(NodeType::Parameter, type, std::move(name)), NodeBase(parent) {}
};

/// Represents a function. A function is a prototype with a list of basic blocks.
class SCATHA(API) Function: public Constant, public NodeWithParent<Function, Module> {
public:
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name);
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string_view name):
        Function(functionType, returnType, parameterTypes, std::string(name)) {}

    Type const* returnType() const { return _returnType; }

    List<Parameter>& parameters() { return params; }
    List<Parameter> const& parameters() const { return params; }

    List<BasicBlock>& basicBlocks() { return bbs; }
    List<BasicBlock> const& basicBlocks() const { return bbs; }

    void addBasicBlock(BasicBlock* basicBlock) {
        basicBlock->set_parent(this);
        bbs.push_back(basicBlock);
    }

private:
    Type const* _returnType;
    List<Parameter> params;
    List<BasicBlock> bbs;
};

/// \p alloca instruction. Allocates automatically managed memory for local variables. Its value is a pointer to the allocated memory.
class SCATHA(API) Alloca: public Instruction {
public:
    explicit Alloca(Context& context, Type const* allocatedType, std::string name);

    Type const* allocatedType() const { return _allocatedType; }

private:
    Type const* _allocatedType;
};

/// Base class of all unary instructions.
class SCATHA(API) UnaryInstruction: public Instruction {
protected:
    explicit UnaryInstruction(NodeType nodeType, Value* operand, Type const* type, std::string name):
        Instruction(nodeType, type, std::move(name)), _operand(operand) {}

public:
    Value* operand() { return _operand; }
    Value const* operand() const { return _operand; }

    Type const* operandType() const { return operand()->type(); }
    
private:
    Value* _operand;
};

/// Load instruction. Load data from memory into a register.
class SCATHA(API) Load: public UnaryInstruction {
public:
    explicit Load(Type const* type, Value* address, std::string name):
        UnaryInstruction(NodeType::Load, address, type, std::move(name)) {
        SC_ASSERT(address->type()->isPointer(), "Address argument to Load must be a pointer");
    }
    Value* address() { return operand(); }
    Value const* address() const { return operand(); }
};

/// Base class of all binary instructions.
class SCATHA(API) BinaryInstruction: public Instruction {
protected:
    explicit BinaryInstruction(NodeType nodeType, Value* lhs, Value* rhs, Type const* type, std::string name = {}):
        Instruction(nodeType, type, std::move(name)), _lhs(lhs), _rhs(rhs) {}

public:
    Value* lhs() { return _lhs; }
    Value const* lhs() const { return _lhs; }
    Value* rhs() { return _rhs; }
    Value const* rhs() const { return _rhs; }

    Type const* operandType() const { return lhs()->type(); }
    
private:
    Value* _lhs;
    Value* _rhs;
};

/// Store instruction. Store a value in a register into memory.
class SCATHA(API) Store: public BinaryInstruction {
public:
    explicit Store(Context& context, Value* address, Value* value);
    
    Value* address() { return lhs(); }
    Value const* address() const { return lhs(); }
    Value* value() { return rhs(); }
    Value const* value() const { return rhs(); }
};

/// Compare instruction.
/// TODO: Rename to 'Compare' or find a uniform naming scheme across the IR module.
class SCATHA(API) CompareInst: public BinaryInstruction {
public:
    explicit CompareInst(Context& context, Value* lhs, Value* rhs, CompareOperation op, std::string name);

    CompareOperation operation() const { return _op; }

private:
    CompareOperation _op;
};

/// Represents a unary arithmetic instruction.
class SCATHA(API) UnaryArithmeticInst: public UnaryInstruction {
public:
    explicit UnaryArithmeticInst(Context& context, Value* operand, UnaryArithmeticOperation op, std::string name);

    UnaryArithmeticOperation operation() const { return _op; }

private:
    UnaryArithmeticOperation _op;
};

/// Represents a binary arithmetic instruction.
class SCATHA(API) ArithmeticInst: public BinaryInstruction {
public:
    explicit ArithmeticInst(Value* lhs, Value* rhs, ArithmeticOperation op, std::string name):
        BinaryInstruction(NodeType::ArithmeticInst, lhs, rhs, lhs->type(), std::move(name)), _op(op) {
        SC_ASSERT(lhs->type() == rhs->type(), "Type mismatch");
    }

    ArithmeticOperation operation() const { return _op; }

private:
    ArithmeticOperation _op;
};

/// Base class for all instructions terminating basic blocks.
class SCATHA(API) TerminatorInst: public Instruction {
protected:
    explicit TerminatorInst(NodeType nodeType, Context& context);
};

/// Goto instruction. Leave the current basic block and unconditionally enter the target basic block.
class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target): TerminatorInst(NodeType::Goto, context), _target(target) {}

    BasicBlock* target() { return _target; }
    BasicBlock const* target() const { return _target; }

private:
    BasicBlock* _target;
};

/// Branch instruction. Leave the current basic block and choose a target basic block based on a condition.
class SCATHA(API) Branch: public TerminatorInst {
public:
    explicit Branch(Context& context, Value* condition, BasicBlock* thenTarget, BasicBlock* elseTarget):
        TerminatorInst(NodeType::Branch, context), _condition(condition), _then(thenTarget), _else(elseTarget) {
        SC_ASSERT(condition->type()->category() == Type::Category::Integral &&
                      static_cast<Integral const*>(condition->type())->bitWidth() == 1,
                  "Condition must be of type i1");
    }

    Value* condition() { return _condition; }
    Value const* condition() const { return _condition; }
    BasicBlock* thenTarget() { return _then; }
    BasicBlock const* thenTarget() const { return _then; }
    BasicBlock* elseTarget() { return _else; }
    BasicBlock const* elseTarget() const { return _else; }

private:
    Value* _condition;
    BasicBlock* _then;
    BasicBlock* _else;
};

/// Return instruction. Return control flow to the calling function.
class SCATHA(API) Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value = nullptr): TerminatorInst(NodeType::Return, context), _value(value) {}

    Value* value() { return _value; }
    Value const* value() const { return _value; }

private:
    Value* _value;
};

/// Function call. Call a function.
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

/// Phi instruction. Choose a value based on where control flow comes from.
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

#endif // SCATHA_IR_CFG_H_

