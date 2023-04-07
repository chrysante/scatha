#ifndef SCATHA_IR_CFG_INSTRUCTION_H_
#define SCATHA_IR_CFG_INSTRUCTION_H_

#include "Common/List.h"
#include "IR/CFG/User.h"
#include "IR/Common.h"

namespace scatha::ir {

/// Base class of all instructions. `Instruction` inherits from `Value` as it
/// (usually) yields a value. If an instruction does not yield a value its
/// `Value` super class is of type void.
class SCATHA_API Instruction:
    public User,
    public NodeWithParent<Instruction, BasicBlock> {
public:
    explicit Instruction(NodeType nodeType,
                         Type const* type,
                         std::string name                            = {},
                         utl::small_vector<Value*> operands          = {},
                         utl::small_vector<Type const*> typeOperands = {});

    /// \returns a view of all instructions using this value.
    ///
    /// \details This casts the elements in
    /// the range returned by `Value::users()` to instructions, as instructions
    /// are only used by other instructions.
    auto users() const {
        return Value::users() |
               ranges::views::transform([]<typename T>(T* user) {
                   return cast<utl::copy_cv_t<T, Instruction>*>(user);
               });
    }

    auto countedUsers() const {
        return Value::countedUsers() |
               ranges::views::transform(
                   []<typename T>(std::pair<T*, uint16_t> user) {
            return std::pair(cast<utl::copy_cv_t<T, Instruction>*>(user.first),
                             user.second);
               });
    }

    /// \returns a view over the type operands of this instruction
    std::span<Type const* const> typeOperands() const { return typeOps; }

    /// Set the type operand at \p index to \p type
    void setTypeOperand(size_t index, Type const* type);

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
    Value* operand() { return operands()[0]; } // namespace scatha::ir

    /// \overload
    Value const* operand() const { return operands()[0]; }

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
    /// \returns the LHS operand
    Value* lhs() { return operands()[0]; }

    ///  \overload
    Value const* lhs() const { return operands()[0]; }

    /// Set LHS operand to \p value
    void setLHS(Value* value) { setOperand(0, value); }

    /// \returns the RHS operand
    Value* rhs() { return operands()[1]; }

    ///  \overload
    Value const* rhs() const { return operands()[1]; }

    /// Set RHS operand to \p value
    void setRHS(Value* value) { setOperand(1, value); }

    /// \returns the type of the operands.
    Type const* operandType() const { return lhs()->type(); }
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_INSTRUCTION_H_
