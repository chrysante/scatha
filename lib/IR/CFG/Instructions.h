#ifndef SCATHA_IR_CFG_INSTRUCTIONS_H_
#define SCATHA_IR_CFG_INSTRUCTIONS_H_

#include <optional>

#include "Common/Ranges.h"
#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Instruction.h"

namespace scatha::ir {

/// `alloca` instruction. Allocates automatically managed memory for local
/// variables. Its value is a pointer to the allocated memory.
class SCATHA_API Alloca: public Instruction {
public:
    explicit Alloca(Context& context,
                    Type const* allocatedType,
                    std::string name);

    explicit Alloca(Context& context,
                    Value* count,
                    Type const* allocatedType,
                    std::string name);

    /// \returns The number of objects allocated.
    Value* count() { return operandAt(0); }

    /// \overload
    Value const* count() const { return operandAt(0); }

    /// \Returns the count as a constant if it is constant
    std::optional<size_t> constantCount() const;

    /// \Returns the allocated size if `count()` is a constant. Otherwise
    /// returns `std::nullopt`
    std::optional<size_t> allocatedSize() const;

    /// Set the number of objects allocated.
    void setCount(Value* count) { setOperand(0, count); }

    /// \returns The type allocated by this `alloca` instruction
    Type const* allocatedType() const { return typeOperandAt(0); }
};

/// `load` instruction. Load data from memory into a register.
class SCATHA_API Load: public UnaryInstruction {
public:
    explicit Load(Value* address, Type const* type, std::string name):
        UnaryInstruction(NodeType::Load, address, type, std::move(name)) {}

    /// \returns the address this instruction loads from.
    Value* address() { return operand(); }

    /// \overload
    Value const* address() const { return operand(); }

    /// Set the address this instruction loads from.
    void setAddress(Value* address);
};

/// `store` instruction. Store a value from a register into memory.
class SCATHA_API Store: public Instruction {
public:
    explicit Store(Context& context, Value* address, Value* value);

    /// \returns the address this store writes to.
    Value* address() { return operands()[0]; }

    /// \overload
    Value const* address() const { return operands()[0]; }

    /// \returns the value written to memory.
    Value* value() { return operands()[1]; }

    /// \overload
    Value const* value() const { return operands()[1]; }

    /// Set the address this instruction stores to.
    void setAddress(Value* address);

    /// Set the value this instruction stores into memory.
    void setValue(Value* value);
};

/// Represents a conversion instruction.
class ConversionInst: public UnaryInstruction {
public:
    explicit ConversionInst(Value* operand,
                            Type const* targetType,
                            Conversion conv,
                            std::string name):
        UnaryInstruction(NodeType::ConversionInst,
                         operand,
                         targetType,
                         std::move(name)),
        conv(conv) {}

    /// \Returns the conversion this instruction performs.
    Conversion conversion() const { return conv; }

    /// \Returns the type of this value and the target type of the conversion.
    ArithmeticType const* type() const;

private:
    Conversion conv;
};

/// `*cmp` instruction.
class SCATHA_API CompareInst: public BinaryInstruction {
public:
    explicit CompareInst(Context& context,
                         Value* lhs,
                         Value* rhs,
                         CompareMode mode,
                         CompareOperation op,
                         std::string name);

    CompareMode mode() const { return _mode; }

    CompareOperation operation() const { return _op; }

    void setOperation(CompareOperation op) { _op = op; }

private:
    CompareMode _mode;
    CompareOperation _op;
};

/// Represents a unary arithmetic instruction.
class SCATHA_API UnaryArithmeticInst: public UnaryInstruction {
public:
    explicit UnaryArithmeticInst(Context& context,
                                 Value* operand,
                                 UnaryArithmeticOperation op,
                                 std::string name);

    UnaryArithmeticOperation operation() const { return _op; }

    void setOperand(Context& context, Value* operand);

private:
    UnaryArithmeticOperation _op;
};

/// Represents a binary arithmetic instruction.
class SCATHA_API ArithmeticInst: public BinaryInstruction {
public:
    explicit ArithmeticInst(Value* lhs,
                            Value* rhs,
                            ArithmeticOperation op,
                            std::string name);

    ArithmeticOperation operation() const { return _op; }

    /// Set LHS operand to \p value
    void setLHS(Value* value) {
        if (!type()) {
            setType(value->type());
        }
        BinaryInstruction::setLHS(value);
    }

    /// Set RHS operand to \p value
    void setRHS(Value* value) {
        if (!type()) {
            setType(value->type());
        }
        BinaryInstruction::setRHS(value);
    }

    void setOperation(ArithmeticOperation op) { _op = op; }

    ArithmeticType const* type() const;

private:
    ArithmeticOperation _op;
};

/// Base class for all instructions terminating basic blocks.
///
/// \details
/// Non-basic block argument are the first `nonTargetArguments` operands.
/// Targets are the following operands.
class SCATHA_API TerminatorInst: public Instruction {
    template <typename T>
    static auto targetsImpl(auto& self) {
        return self.operands() | ranges::views::drop(self.nonTargetArguments) |
               CastOrNull<T*>;
    }

public:
    auto targets() { return targetsImpl<BasicBlock>(*this); }

    auto targets() const { return targetsImpl<BasicBlock const>(*this); }

    BasicBlock* targetAt(size_t index) {
        return const_cast<BasicBlock*>(std::as_const(*this).targetAt(index));
    }

    BasicBlock const* targetAt(size_t index) const;

    size_t numTargets() const { return targets().size(); }

    void updateTarget(BasicBlock* oldTarget, BasicBlock* newTarget);

    void setTarget(size_t index, BasicBlock* bb);

protected:
    explicit TerminatorInst(NodeType nodeType,
                            Context& context,
                            std::initializer_list<Value*> operands,
                            std::initializer_list<BasicBlock*> targets);

private:
    uint16_t nonTargetArguments = 0;
};

/// `goto` instruction. Leave the current basic block and unconditionally enter
/// the target basic block.
class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target):
        TerminatorInst(NodeType::Goto, context, {}, { target }) {}

    BasicBlock* target() { return targets()[0]; }
    BasicBlock const* target() const { return targets()[0]; }

    void setTarget(BasicBlock* bb) { setOperand(0, bb); }
};

/// `branch` instruction. Leave the current basic block and choose a target
/// basic block based on a condition.
///
/// \details
/// Condition is the first operand. Targets are second and third operands.
class SCATHA_API Branch: public TerminatorInst {
public:
    explicit Branch(Context& context,
                    Value* condition,
                    BasicBlock* thenTarget,
                    BasicBlock* elseTarget):
        TerminatorInst(NodeType::Branch,
                       context,
                       { condition },
                       { thenTarget, elseTarget }) {}

    Value* condition() { return operands()[0]; }
    Value const* condition() const { return operands()[0]; }

    BasicBlock* thenTarget() { return targets()[0]; }
    BasicBlock const* thenTarget() const { return targets()[0]; }

    BasicBlock* elseTarget() { return targets()[1]; }
    BasicBlock const* elseTarget() const { return targets()[1]; }

    void setCondition(Value* cond) { setOperand(0, cond); }

    void setThenTarget(BasicBlock* bb) { setOperand(1, bb); }

    void setElseTarget(BasicBlock* bb) { setOperand(2, bb); }
};

/// `return` instruction. Return control flow to the calling function.
///
/// \details
class SCATHA_API Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value):
        TerminatorInst(NodeType::Return, context, { value }, {}) {}

    explicit Return(Context& context);

    /// Value returned by this return instruction.
    /// If the parent function returns void, this is an unspecified value of
    /// type void.
    Value* value() {
        return const_cast<Value*>(static_cast<Return const*>(this)->value());
    }

    /// \overload
    Value const* value() const { return operands()[0]; }

    /// Set the value returned by this `return` instruction.
    void setValue(Value* newValue) { setOperand(0, newValue); }
};

/// `call` instruction. Calls a function.
///
/// \details
/// Callee is stored as the first operand. Arguments are the following operands
/// starting from index 1.
class SCATHA_API Call: public Instruction {
public:
    /// Construct call to function without arguments
    explicit Call(Type const* returnType,
                  Value* function,
                  std::string name = {});

    /// Construct call to arbitrary target with return type
    explicit Call(Type const* returnType,
                  Value* function,
                  std::span<Value* const> arguments,
                  std::string name = {});

    /// Construct statically bound call with return type deduced form the
    /// function
    explicit Call(Callable* function,
                  std::span<Value* const> arguments,
                  std::string name = {});

    /// \Returns the called function
    Value* function() {
        return const_cast<Value*>(std::as_const(*this).function());
    }

    /// \overload
    Value const* function() const { return operandAt(0); }

    /// Set the called function to \p function
    void setFunction(Value* function);

    /// \Returns a view over the arguments of this function call
    auto arguments() { return operands() | ranges::views::drop(1); }

    /// \overload
    auto arguments() const { return operands() | ranges::views::drop(1); }

    /// \Returns the argument at position \p index
    Value* argumentAt(size_t index) { return operandAt(index + 1); }

    /// \overload
    Value const* argumentAt(size_t index) const { return operandAt(index + 1); }

    /// Sets the argument at position \p index to \p value
    void setArgument(size_t index, Value* value) {
        setOperand(1 + index, value);
    }
};

/// `phi` instruction. Select a value based on where control flow comes from.
class SCATHA_API Phi: public Instruction {
public:
    /// \overload
    explicit Phi(std::initializer_list<PhiMapping> args, std::string name):
        Phi(std::span<PhiMapping const>(args), std::move(name)) {}

    /// Construct a phi node with a set of arguments.
    explicit Phi(std::span<PhiMapping const> args, std::string name):
        Phi(nullptr /* Type will be set by call to setArguments() */,
            std::move(name)) {
        setArguments(args);
    }

    /// Construct a phi node with a \p count arguments
    explicit Phi(Type const* type, size_t count, std::string name):
        Phi(type, std::move(name)) {
        setOperandCount(count);
        _preds.resize(count);
    }

    /// Construct an empty phi node.
    explicit Phi(Type const* type, std::string name):
        Instruction(NodeType::Phi, type, std::move(name), {}) {}

    /// Assign arguments to this phi node.
    void setArguments(std::span<PhiMapping const> args);

    /// Assign \p value to the predecessor argument \p pred
    /// \pre \p pred must be a predecessor to this phi node.
    void setArgument(BasicBlock const* pred, Value* value);

    /// Assign \p value to argument at \p index
    void setArgument(size_t index, Value* value);

    /// Assign \p pred to predecessor at \p index
    void setPredecessor(size_t index, BasicBlock* pred);

    /// Append an argument to this phi node. Use this to adjust this phi node
    /// after adding predecessors to its parent basic block.
    void addArgument(BasicBlock* pred, Value* value);

    /// Number of arguments. Must match the number of predecessors of parent
    /// basic block.
    size_t argumentCount() const { return _preds.size(); }

    /// Access argument pair at index \p index
    PhiMapping argumentAt(size_t index) {
        SC_EXPECT(index < argumentCount());
        return { _preds[index], operands()[index] };
    }

    /// \overload
    ConstPhiMapping argumentAt(size_t index) const {
        SC_EXPECT(index < argumentCount());
        return { _preds[index], operands()[index] };
    }

    Value* operandOf(BasicBlock const* pred) {
        return const_cast<Value*>(
            static_cast<Phi const*>(this)->operandOf(pred));
    }

    Value const* operandOf(BasicBlock const* pred) const;

    /// View over all incoming edges. Must be the same as predecessors of parent
    /// basic block.
    std::span<BasicBlock* const> incomingEdges() { return _preds; }

    /// \overload
    std::span<BasicBlock const* const> incomingEdges() const { return _preds; }

    /// View over arguments
    auto arguments() {
        return ranges::views::zip(_preds, operands()) |
               ranges::views::transform([](auto p) -> PhiMapping { return p; });
    }

    /// \overload
    auto arguments() const {
        return ranges::views::zip(_preds, operands()) |
               ranges::views::transform(
                   [](auto p) -> ConstPhiMapping { return p; });
    }

    auto indexedArguments() {
        return ranges::views::zip(ranges::views::iota(size_t{ 0 }),
                                  _preds,
                                  operands());
    }

    auto indexedArguments() const {
        return ranges::views::zip(ranges::views::iota(size_t{ 0 }),
                                  _preds,
                                  operands());
    }

    size_t indexOf(BasicBlock const* predecessor) const {
        return utl::narrow_cast<size_t>(
            std::find(_preds.begin(), _preds.end(), predecessor) -
            _preds.begin());
    }

    /// Remove the argument corresponding to \p *predecessor
    /// \p *predecessor must be an argument of this phi instruction.
    void removeArgument(BasicBlock const* predecessor) {
        removeArgument(indexOf(predecessor));
    }

    /// Remove the argument at index \p index
    void removeArgument(size_t index) {
        _preds.erase(_preds.begin() + index);
        removeOperand(index);
    }

private:
    utl::small_vector<BasicBlock*> _preds;
};

/// Select one of two values basec on a boolean condition
class Select: public Instruction {
public:
    explicit Select(Value* condition,
                    Value* thenValue,
                    Value* elseValue,
                    std::string name);

    /// The condition to select on.
    Value* condition() { return operandAt(0); }

    /// \overload
    Value const* condition() const { return operandAt(0); }

    /// Set the condition to select on.
    void setCondition(Value* value) { setOperand(0, value); }

    /// Value to choose if condition is `true`
    Value* thenValue() { return operandAt(1); }

    /// \overload
    Value const* thenValue() const { return operandAt(1); }

    /// Set the value to choose if condition is `true`.
    void setThenValue(Value* value) {
        if (!type()) {
            setType(value->type());
        }
        setOperand(1, value);
    }

    /// Value to choose if condition is `false`
    Value* elseValue() { return operandAt(2); }

    /// \overload
    Value const* elseValue() const { return operandAt(2); }

    /// Set the value to choose if condition is `false`.
    void setElseValue(Value* value) {
        if (!type()) {
            setType(value->type());
        }
        setOperand(2, value);
    }
};

/// Common base class of `GetElementPointer`, `ExtractValue` and `InsertValue`
class AccessValueInst: public Instruction {
public:
    using Instruction::Instruction;

    /// The constant member indices. For an `insert_value` or `extract_value`
    /// instruction these are all the indices. For a `getelementptr`
    /// instruction these are the constant indices excluding the array index
    std::span<size_t const> memberIndices() const { return _indices; }

    /// Adds the constant member index \p index to the front of the current
    /// member indices
    void addMemberIndexFront(size_t index) {
        _indices.insert(_indices.begin(), index);
    }

    /// Adds the constant member index \p index to the back of the current
    /// member indices
    void addMemberIndexBack(size_t index) { _indices.push_back(index); }

    /// Set the constant member indices to \p indices
    void setMemberIndices(std::span<size_t const> indices);

private:
    utl::small_vector<size_t> _indices;
};

/// `getelementptr` instruction. Calculate offset pointer to a structure member
/// or array element.
class GetElementPointer: public AccessValueInst {
public:
    explicit GetElementPointer(Context& context,
                               Type const* inboundsType,
                               Value* basePointer,
                               Value* arrayIndex,
                               std::initializer_list<size_t> memberIndices,
                               std::string name):
        GetElementPointer(context,
                          inboundsType,
                          basePointer,
                          arrayIndex,
                          std::span<size_t const>(memberIndices),
                          name) {}

    explicit GetElementPointer(Context& context,
                               Type const* inboundsType,
                               Value* basePointer,
                               Value* arrayIndex,
                               std::span<size_t const> memberIndices,
                               std::string name);

    /// \Returns the type of the value that the base pointer points to.
    Type const* inboundsType() const { return typeOperands()[0]; }

    /// \Returns the type of the value the result of this instruction points to.
    Type const* accessedType() const;

    /// \Returns the pointer being modified by this instruction
    Value* basePointer() { return operandAt(0); }

    /// \overload
    Value const* basePointer() const { return operandAt(0); }

    /// \Returns the dynamic array index operand
    Value* arrayIndex() { return operandAt(1); }

    /// \overload
    Value const* arrayIndex() const { return operandAt(1); }

    /// \Returns `true` if the array index is a constant
    bool hasConstantArrayIndex() const;

    /// \Returns the array index as a constant if possible
    std::optional<size_t> constantArrayIndex() const;

    ///  \Returns the constant byte offset that this instruction computes if the
    /// array index is a constant
    std::optional<size_t> constantByteOffset() const;

    ///  \Returns the constant byte inner byte offset, that is the offset
    /// computed by the constant member indices, not included the dynamic array
    /// index
    size_t innerByteOffset() const;

    /// Sets the inbounds type to \p type
    void setInboundsType(Type const* type) { setTypeOperand(0, type); }

    /// Sets the base pointer to \p pointer
    void setBasePtr(Value* pointer) { setOperand(0, pointer); }

    /// Sets the array index to \p index
    void setArrayIndex(Value* index) { setOperand(1, index); }
};

/// `extract_value` instruction. Extract the value of a structure member or
/// array element.
class ExtractValue: public AccessValueInst {
public:
    explicit ExtractValue(Value* baseValue,
                          std::initializer_list<size_t> indices,
                          std::string name):
        ExtractValue(baseValue,
                     std::span<size_t const>(indices),
                     std::move(name)) {}

    explicit ExtractValue(Value* baseValue,
                          std::span<size_t const> indices,
                          std::string name);

    /// The record being accessed
    Value* baseValue() { return operandAt(0); }

    /// \overload
    Value const* baseValue() const { return operandAt(0); }

    /// Set the record being accessed to \p value
    void setBaseValue(Value* value);
};

/// `insert_value` instruction. Insert a value into a structure or array.
class InsertValue: public AccessValueInst {
public:
    explicit InsertValue(Value* baseValue,
                         Value* insertedValue,
                         std::initializer_list<size_t> indices,
                         std::string name):
        InsertValue(baseValue,
                    insertedValue,
                    std::span<size_t const>(indices),
                    std::move(name)) {}

    explicit InsertValue(Value* baseValue,
                         Value* insertedValue,
                         std::span<size_t const> indices,
                         std::string name);

    /// \Returns the record being accessed
    Value* baseValue() { return operandAt(0); }

    /// \overload
    Value const* baseValue() const { return operandAt(0); }

    /// Set the record being accessed to \p value
    void setBaseValue(Value* value);

    /// \Returns the value being inserted
    Value* insertedValue() { return operandAt(1); }

    /// \overload
    Value const* insertedValue() const { return operandAt(1); }

    /// Set the value being inserted to \p value
    void setInsertedValue(Value* value) { setOperand(1, value); }
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_INSTRUCTIONS_H_
