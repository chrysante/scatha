#ifndef SCATHA_IR_CFG_H_
#define SCATHA_IR_CFG_H_

#include <string>

#include <utl/ranges.hpp>
#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "IR/Common.h"
#include "IR/List.h"
#include "IR/Type.h"

namespace scatha::ir {

class Context;
class Module;

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class SCATHA(API) Value {
protected:
    explicit Value(NodeType nodeType, Type const* type, std::string name = {}) noexcept:
        _nodeType(nodeType), _type(type), _name(std::move(name)) {}

    /// For complex initialization.
    void setType(Type const* type) { _type = type; }

public:
    /// The runtime type of this CFG node.
    NodeType nodeType() const { return _nodeType; }

    /// The type of this value.
    Type const* type() const { return _type; }

    /// The name of this value.
    std::string_view name() const { return _name; }

    /// Wether this value is named.
    bool hasName() const { return !_name.empty(); }

    /// Set the name of this value.
    void setName(std::string name) { _name = std::move(name); }

    /// View of all users using this value.
    auto users() const { return utl::transform(_users, [](auto&& p) { return p.first; }); }

    /// Number of users using this value. Multiple uses by the same user are counted as one.
    size_t userCount() const { return _users.size(); }
    
    /// Register a user of this value.
    void addUser(User* user);
    
    /// Unregister a user of this value.
    void removeUser(User* user);
    
private:
    NodeType _nodeType;
    Type const* _type;
    std::string _name;
    utl::hashmap<User*, uint16_t> _users;
};

/// For dyncast compatibilty of the CFG
inline NodeType dyncast_get_type(std::derived_from<Value> auto const& value) {
    return value.nodeType();
}

/// Represents a user of values
class SCATHA(API) User: public Value {
public:
    std::span<Value* const> operands() { return _operands; }
    std::span<Value const* const> operands() const { return _operands; }
    
    void setOperand(size_t index, Value* operand);
    
protected:
    explicit User(NodeType nodeType, Type const* type, std::string name, std::initializer_list<Value*> operands):
        User(nodeType, type, std::move(name), std::span<Value* const>(operands)) {}
    
    explicit User(NodeType nodeType, Type const* type, std::string name = {}, std::span<Value* const> operands = {});
    
private:
    utl::small_vector<Value*> _operands;
};

/// Represents a (global) constant value.
class SCATHA(API) Constant: public User {
public:
    using User::User;

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

/// Represents a global floating point constant value.
class SCATHA(API) FloatingPointConstant: public Constant {
public:
    explicit FloatingPointConstant(Context& context, APFloat value, size_t bitWidth);

    APFloat const& value() const { return _value; }

private:
    APFloat _value;
};

/// Base class of all instructions. Every instruction inherits from \p Value as it (usually) yields a value. If an
/// instruction does not yield a value its \p Value super class is of type void.
class SCATHA(API) Instruction: public User, public NodeWithParent<Instruction, BasicBlock> {
protected:
    using User::User;
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

    /// Check wether \p inst is an instruction of this basic block.
    bool contains(Instruction const& inst) const;
    
    /// Returns the terminator instruction if this basic block is well formed or nullptr
    TerminatorInst const* terminator() const;
    
    /// \overload
    TerminatorInst* terminator() {
        return const_cast<TerminatorInst*>(static_cast<BasicBlock const*>(this)->terminator());
    }
    
    /// This is exposed directly because some algorithms need to erase instructions from here.
    List<Instruction> instructions;
    
    /// Also just expose this directly, be careful though.
    utl::small_vector<BasicBlock*> predecessors, successors;
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

/// \p alloca instruction. Allocates automatically managed memory for local variables. Its value is a pointer to the
/// allocated memory.
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
        Instruction(nodeType, type, std::move(name), { operand }) {}

public:
    Value* operand() { return operands()[0]; }
    Value const* operand() const { return operands()[0]; }

    Type const* operandType() const { return operand()->type(); }
};

/// Load instruction. Load data from memory into a register.
class SCATHA(API) Load: public UnaryInstruction {
public:
    explicit Load(Value* address, std::string name):
        UnaryInstruction(NodeType::Load,
                         address,
                         cast<PointerType const*>(address->type())->pointeeType(),
                         std::move(name)) {}

    Value* address() { return operand(); }
    Value const* address() const { return operand(); }
};

/// Base class of all binary instructions.
class SCATHA(API) BinaryInstruction: public Instruction {
protected:
    explicit BinaryInstruction(NodeType nodeType, Value* lhs, Value* rhs, Type const* type, std::string name = {}):
        Instruction(nodeType, type, std::move(name), { lhs, rhs }) {}

public:
    Value* lhs() { return operands()[0]; }
    Value const* lhs() const { return operands()[0]; }
    Value* rhs() { return operands()[1]; }
    Value const* rhs() const { return operands()[1]; }

    Type const* operandType() const { return lhs()->type(); }
};

/// Store instruction. Store a value from a register into memory.
class SCATHA(API) Store: public BinaryInstruction {
public:
    explicit Store(Context& context, Value* dest, Value* source);

    Value* dest() { return lhs(); }
    Value const* dest() const { return lhs(); }
    Value* source() { return rhs(); }
    Value const* source() const { return rhs(); }
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
    explicit ArithmeticInst(Value* lhs, Value* rhs, ArithmeticOperation op, std::string name);

    ArithmeticOperation operation() const { return _op; }

private:
    ArithmeticOperation _op;
};

/// Base class for all instructions terminating basic blocks.
class SCATHA(API) TerminatorInst: public Instruction {
protected:
    explicit TerminatorInst(NodeType nodeType, Context& context, std::initializer_list<Value*> operands = {});
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
        TerminatorInst(NodeType::Branch, context, { condition }), _then(thenTarget), _else(elseTarget) {
        SC_ASSERT(cast<IntegralType const*>(condition->type())->bitWidth() == 1, "Condition must be of type i1");
    }

    Value* condition() { return operands()[0]; }
    Value const* condition() const { return operands()[0]; }
    BasicBlock* thenTarget() { return _then; }
    BasicBlock const* thenTarget() const { return _then; }
    BasicBlock* elseTarget() { return _else; }
    BasicBlock const* elseTarget() const { return _else; }

private:
    BasicBlock* _then;
    BasicBlock* _else;
};

/// Return instruction. Return control flow to the calling function.
class SCATHA(API) Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value = nullptr):
        TerminatorInst(NodeType::Return, context, { value }) {}

    /// May be null in case of a void function
    Value* value() { return const_cast<Value*>(static_cast<Return const*>(this)->value()); }
    
    /// May be null in case of a void function
    Value const* value() const { return operands().empty() ? nullptr : operands()[0]; }
};

/// Function call. Call a function.
class SCATHA(API) FunctionCall: public Instruction {
public:
    explicit FunctionCall(Function* function, std::span<Value* const> arguments, std::string name = {});

    Function* function() { return _function; }
    Function const* function() const { return _function; }

    std::span<Value* const> arguments() { return operands(); }
    std::span<Value const* const> arguments() const { return operands(); }

private:
    Function* _function;
};

/// External function call. Call an external function.
class SCATHA(API) ExtFunctionCall: public Instruction {
public:
    explicit ExtFunctionCall(size_t slot,
                             size_t index,
                             std::span<Value* const> arguments,
                             ir::Type const* returnType,
                             std::string name = {});

    size_t slot() const { return _slot; }

    size_t index() const { return _index; }

    std::span<Value* const> arguments() { return operands(); }
    std::span<Value const* const> arguments() const { return operands(); }

private:
    u32 _slot, _index;
};

struct PhiMapping {
    PhiMapping(BasicBlock* pred, Value* value): pred(pred), value(value) {}
    
    BasicBlock* pred;
    Value* value;
};

struct ConstPhiMapping {
    ConstPhiMapping(BasicBlock const* pred, Value const* value): pred(pred), value(value) {}
    
    ConstPhiMapping(PhiMapping p): pred(p.pred), value(p.value) {}
    
    BasicBlock const* pred;
    Value const* value;
};

/// Phi instruction. Choose a value based on where control flow comes from.
class SCATHA(API) Phi: public Instruction {
public:
    explicit Phi(std::initializer_list<PhiMapping> args, std::string name):
        Phi(std::span<PhiMapping const>(args), std::move(name)) {}
    explicit Phi(std::span<PhiMapping const> args, std::string name);

    /// Note: This ugly interface could be changed if we had C++20 zip range.
    
    size_t argumentCount() const { return _preds.size(); }
    
    PhiMapping argumentAt(size_t index) {
        SC_ASSERT(index < argumentCount(), "");
        return { _preds[index], operands()[index] };
    }
    
    ConstPhiMapping argumentAt(size_t index) const {
        SC_ASSERT(index < argumentCount(), "");
        return { _preds[index], operands()[index] };
    }
    
private:
    utl::small_vector<BasicBlock*> _preds;
};

/// GetElementPointer instruction. Calculate offset pointer to a structure member or array element.
class GetElementPointer: public Instruction {
public:
    explicit GetElementPointer(
        Context& context, Type const* accessedType, Value* basePointer, size_t offsetIndex, std::string name = {});

    Type const* accessedType() const { return accType; }

    Value* basePointer() { return operands()[0]; }
    Value const* basePointer() const { return operands()[0]; }

    size_t offsetIndex() const { return offsetIdx; }

private:
    Type const* accType;
    size_t offsetIdx;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_H_
