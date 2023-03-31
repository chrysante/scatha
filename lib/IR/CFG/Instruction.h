#ifndef SCATHA_IR_CFG_INSTRUCTION_H_
#define SCATHA_IR_CFG_INSTRUCTION_H_

#include "IR/CFG/List.h"
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

    /// \returns a view over the type operands of this instruction
    std::span<Type const* const> typeOperands() const { return typeOps; }

    /// Set the type operand at \p index to \p type
    void setTypeOperand(size_t index, Type const* type);

protected:
    using User::User;
    utl::small_vector<Type const*> typeOps;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_INSTRUCTION_H_
