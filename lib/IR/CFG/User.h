#ifndef SCATHA_IR_CFG_USER_H_
#define SCATHA_IR_CFG_USER_H_

#include <string>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/CFG/Value.h"
#include "IR/Common.h"

namespace scatha::ir {

/// Represents a user of values
class SCATHA_API User: public Value {
public:
    /// \returns a view of all operands
    std::span<Value* const> operands() { return _operands; }

    /// \overload
    std::span<Value const* const> operands() const { return _operands; }

    /// Set the operand at \p index to \p operand
    /// Updates user list of \p operand and removed operand.
    /// \p operand may be `nullptr`.
    void setOperand(size_t index, Value* operand);

    /// Find \p oldOperand and replace it with \p newOperand
    /// User lists are updated.
    /// \pre \p oldOperand must be an operand of this user.
    void updateOperand(Value const* oldOperand, Value* newOperand);

    /// Set all operands to `nullptr`.
    /// User lists are updated.
    void clearOperands();

    /// \returns `true` iff \p value is an operand of this user.
    bool directlyUses(Value const* value) const;

protected:
    explicit User(NodeType nodeType,
                  Type const* type,
                  std::string name                   = {},
                  utl::small_vector<Value*> operands = {});

    /// Clear all operands and replace with new operands.
    /// User lists are updated.
    void setOperands(utl::small_vector<Value*> operands);

    void setOperandCount(size_t count) { _operands.resize(count); }

    /// Append an operand to the end of our operand list.
    void addOperand(Value* op);

    /// Remove operand at \p index
    /// User lists are updated.
    /// \warning Erases the operand, that means higher indices get messed up.
    void removeOperand(size_t index);

private:
    friend class BinaryInstruction;
    utl::small_vector<Value*> _operands;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_USER_H_
