#ifndef SCATHA_IR_CFG_INSTRUCTION_H_
#define SCATHA_IR_CFG_INSTRUCTION_H_

#include "Common/List.h"
#include "Common/Ranges.h"
#include "IR/CFG/User.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Base class of all instructions. `Instruction` inherits from `Value` as it
/// (usually) yields a value. If an instruction does not yield a value its
/// `Value` super class is of type void.
class SCATHA_API Instruction:
    public User,
    public ListNode<Instruction>,
    public ParentedNode<BasicBlock> {
public:
    explicit Instruction(NodeType nodeType,
                         Type const* type,
                         std::string name = {},
                         utl::small_vector<Value*> operands = {},
                         utl::small_vector<Type const*> typeOperands = {});

    /// \returns a view of all instructions using this value.
    ///
    /// \details This casts the elements in
    /// the range returned by `Value::users()` to instructions, as instructions
    /// are only used by other instructions.
    auto users() const {
        return Value::users() | ranges::views::transform(cast<Instruction*>);
    }

    /// \returns a view over the type operands of this instruction
    std::span<Type const* const> typeOperands() const { return typeOps; }

    /// \returns the type operand of this instruction at index \p index
    Type const* typeOperandAt(size_t index) const { return typeOps[index]; }

    /// Set the type operand at \p index to \p type
    void setTypeOperand(size_t index, Type const* type);

    /// The function that owns this instruction
    Function* parentFunction() {
        return const_cast<Function*>(
            static_cast<Instruction const*>(this)->parentFunction());
    }

    /// \overload
    Function const* parentFunction() const;

protected:
    using User::User;
    utl::small_vector<Type const*> typeOps;
};

/// Base class of all unary instructions.
class SCATHA_API UnaryInstruction: public Instruction {
protected:
    explicit UnaryInstruction(NodeType nodeType,
                              Value* operand,
                              Type const* type,
                              std::string name):
        Instruction(nodeType, type, std::move(name), { operand }) {}

public:
    /// \returns the operand of this instruction
    Value* operand() { return operandAt(0); }

    /// \overload
    Value const* operand() const { return operandAt(0); }

    /// Set the single operand of this unary instruction.
    void setOperand(Value* value) { User::setOperand(0, value); }

    /// \returns the type of the operand of this instruction
    Type const* operandType() const { return operand()->type(); }
};

/// Base class of all binary instructions.
class SCATHA_API BinaryInstruction: public Instruction {
protected:
    explicit BinaryInstruction(NodeType nodeType,
                               Value* lhs,
                               Value* rhs,
                               Type const* type,
                               std::string name):
        Instruction(nodeType, type, std::move(name), { lhs, rhs }) {}

public:
    /// Swap `lhs()` and `rhs()` operands of this binary instruction
    void swapOperands();

    /// \returns the LHS operand
    Value* lhs() { return operandAt(0); }

    ///  \overload
    Value const* lhs() const { return operandAt(0); }

    /// Set LHS operand to \p value
    void setLHS(Value* value) { setOperand(0, value); }

    /// \returns the RHS operand
    Value* rhs() { return operandAt(1); }

    ///  \overload
    Value const* rhs() const { return operandAt(1); }

    /// Set RHS operand to \p value
    void setRHS(Value* value) { setOperand(1, value); }

    /// \returns the type of the operands.
    Type const* operandType() const { return lhs()->type(); }
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_INSTRUCTION_H_
