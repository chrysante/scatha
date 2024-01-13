#ifndef SCATHA_IR_CFG_USER_H_
#define SCATHA_IR_CFG_USER_H_

#include <optional>
#include <span>
#include <string>

#include <utl/vector.hpp>

#include "Common/Base.h"
#include "IR/CFG/Value.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a user of values
class SCATHA_API User: public Value {
public:
    /// Calls `clearOperands()`
    ~User();

    /// \returns a view of all operands
    std::span<Value* const> operands() { return _operands; }

    /// \overload
    std::span<Value const* const> operands() const { return _operands; }

    /// \returns The operand at index \p index
    Value* operandAt(size_t index) { return _operands[index]; }

    /// \overload
    Value const* operandAt(size_t index) const { return _operands[index]; }

    /// \Returns the (first) index of the operand \p operand
    std::optional<size_t> indexOf(Value const* operand) const;

    /// \Returns the number of operands of this user
    size_t numOperands() const { return operands().size(); }

    /// Set the operand at \p index to \p operand
    /// Updates user list of \p operand and removed operand.
    /// \p operand may be `nullptr`.
    void setOperand(size_t index, Value* operand);

    /// Replaces all uses of \p oldOperand with \p newOperand
    /// User lists are updated.
    /// \pre \p oldOperand must be an operand of this user.
    void updateOperand(Value const* oldOperand, Value* newOperand);

    /// Replaces all uses of \p oldOperand with \p newOperand
    /// User lists are updated.
    /// \Returns `true` if any operands have been updated.
    bool tryUpdateOperand(Value const* oldOperand, Value* newOperand);

    /// Set all operands to `nullptr`.
    /// User lists are updated.
    void clearOperands();

    /// \returns `true` if \p value is an operand of this user.
    bool directlyUses(Value const* value) const;

protected:
    explicit User(NodeType nodeType,
                  Type const* type,
                  std::string name = {},
                  std::span<Value* const> operands = {});

    /// Clear all operands and replace with new operands.
    /// User lists are updated.
    void setOperands(std::span<Value* const> operands);

    void setOperandCount(size_t count) { _operands.resize(count); }

    /// Append an operand to the end of our operand list.
    void addOperand(Value* op);

    /// Remove operand at \p index
    /// User lists are updated.
    /// \warning Erases the operand, that means higher indices get messed up.
    void removeOperand(size_t index);

private:
    friend class BinaryInstruction;
    utl::small_vector<Value*, 2> _operands;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_USER_H_
