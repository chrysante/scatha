#ifndef SCATHA_IR_CFG_H_
#define SCATHA_IR_CFG_H_

#include <string>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hash.hpp>
#include <utl/hashmap.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/UniquePtr.h"
#include "IR/Common.h"
#include "IR/Iterator.h"
#include "IR/List.h"
#include "IR/Type.h"
#include "IR/UniqueName.h"

namespace scatha::ir {

class Context;
class Module;

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class SCATHA_API Value {
protected:
    explicit Value(NodeType nodeType,
                   Type const* type,
                   std::string name) noexcept:
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
    void setName(std::string name);

    /// View of all users using this value.
    auto users() const {
        return _users |
               ranges::views::transform([](auto&& p) { return p.first; });
    }

    /// Number of users using this value. Multiple uses by the same user are
    /// counted as one.
    size_t userCount() const { return _users.size(); }

private:
    friend class User;

    /// Register a user of this value. Won't affect \p user
    /// This function and `removeUserWeak()` are solely intended to be used be
    /// class `User`
    void addUserWeak(User* user);

    /// Unregister a user of this value. `this` will _not_ be cleared from the
    /// operand list of \p user
    void removeUserWeak(User* user);

    friend class BasicBlock;
    friend class Function;

    /// Unique the existing name of this value. This should be called when
    /// adding this value to a function.
    void uniqueExistingName(Function& func);

    /// Customization point for `UniquePtr`
    friend void scatha::internal::privateDelete(Value* value);

    /// Customization point for `ir::DynAllocator`
    friend void scatha::internal::privateDestroy(Value* value);

private:
    NodeType _nodeType;
    Type const* _type;
    std::string _name;
    utl::hashmap<User*, uint16_t> _users;
};

/// For `dyncast` compatibilty of the CFG
inline NodeType dyncast_get_type(std::derived_from<Value> auto const& value) {
    return value.nodeType();
}

/// Represents a user of values
class SCATHA_API User: public Value {
public:
    /// \returns a view of all operands
    std::span<Value* const> operands() { return _operands; }

    /// \overload
    std::span<Value const* const> operands() const { return _operands; }

    /// Set the operand at \p index to \p operand
    /// Updates user list of \p operand and removed operand.
    void setOperand(size_t index, Value* operand);

    /// Clear all operands and replace with new operands.
    /// User lists are updated.
    void setOperands(utl::small_vector<Value*> operands);

    /// Find \p oldOperand and replace it with \p newOperand
    /// User lists are updated.
    /// \pre \p oldOperand must be an operand of this user.
    void updateOperand(Value const* oldOperand, Value* newOperand);

    /// Remove operand at \p index
    /// User lists are updated.
    void removeOperand(size_t index);

    /// Remove all operands from this user.
    /// User lists are updated.
    void clearOperands();

protected:
    explicit User(NodeType nodeType,
                  Type const* type,
                  std::string name                   = {},
                  utl::small_vector<Value*> operands = {});

private:
    utl::small_vector<Value*> _operands;
};

/// Represents a (global) constant value.
class SCATHA_API Constant: public User {
public:
    using User::User;

private:
};

/// Represents a global integral constant value.
class SCATHA_API IntegralConstant: public Constant {
public:
    explicit IntegralConstant(Context& context, APInt value, size_t bitWidth);

    APInt const& value() const { return _value; }

private:
    APInt _value;
};

/// Represents a global floating point constant value.
class SCATHA_API FloatingPointConstant: public Constant {
public:
    explicit FloatingPointConstant(Context& context,
                                   APFloat value,
                                   size_t bitWidth);

    APFloat const& value() const { return _value; }

private:
    APFloat _value;
};

/// Represents an `undef` value.
class SCATHA_API UndefValue: public Constant {
public:
    explicit UndefValue(Type const* type):
        Constant(NodeType::UndefValue, type) {}
};

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

protected:
    using User::User;
    utl::small_vector<Type const*> typeOps;
};

namespace internal {

/// Base class of `BasicBlock` and `Function` implementing their common
/// container-like interface. `ValueType` is `Instruction` for `BasicBlock` and
/// `BasicBlock` for `Function`.
template <typename Derived, typename ValueType>
class CFGList {
public:
    using Iterator      = typename List<ValueType>::iterator;
    using ConstIterator = typename List<ValueType>::const_iterator;

    /// Callee takes ownership.
    void pushFront(ValueType* value) { insert(values.begin(), value); }

    /// \overload
    void pushFront(UniquePtr<ValueType> value) { pushFront(value.release()); }

    /// Callee takes ownership.
    void pushBack(ValueType* value) { insert(values.end(), value); }

    /// \overload
    void pushBack(UniquePtr<ValueType> value) { pushBack(value.release()); }

    /// Callee takes ownership.
    Iterator insert(ConstIterator before, ValueType* value) {
        asDerived().insertCallback(*value);
        return values.insert(before, value);
    }

    /// \overload
    ValueType* insert(ValueType const* before, ValueType* value) {
        asDerived().insertCallback(*value);
        return insert(ConstIterator(before), value).to_address();
    }

    /// Merge `*this` with \p *rhs
    /// Insert nodes of \p *rhs before \p pos
    void splice(ConstIterator pos, Derived* rhs) {
        splice(pos, rhs->begin(), rhs->end());
    }

    /// Merge range `[begin, end)` into `*this`
    /// Insert nodes before \p pos
    void splice(ConstIterator pos, Iterator first, ConstIterator last) {
        for (auto itr = first; itr != last; ++itr) {
            SC_ASSERT(itr->parent() != this, "This is UB");
            asDerived().insertCallback(*itr);
        }
        values.splice(pos, first, last);
    }

    /// Erase an instruction. Clears the operands.
    Iterator erase(ConstIterator position) {
        SC_ASSERT(position->users().empty(),
                  "We should not erase this value when it's still in use");
        asDerived().eraseCallback(*position);
        return values.erase(position);
    }

    /// \overload
    Iterator erase(ValueType const* value) {
        return erase(ConstIterator(value));
    }

    /// \overload
    Iterator erase(ConstIterator first, ConstIterator last) {
        for (auto itr = first; itr != last; ++itr) {
            SC_ASSERT(itr->parent() == this, "This is UB");
            asDerived().eraseCallback(*itr);
        }
        return values.erase(first, last);
    }

    Iterator begin() { return values.begin(); }
    ConstIterator begin() const { return values.begin(); }

    auto rbegin() { return values.rbegin(); }
    auto rbegin() const { return values.rbegin(); }

    Iterator end() { return values.end(); }
    ConstIterator end() const { return values.end(); }

    auto rend() { return values.rend(); }
    auto rend() const { return values.rend(); }

    bool empty() const { return values.empty(); }

    ValueType& front() { return values.front(); }
    ValueType const& front() const { return values.front(); }

    ValueType& back() { return values.back(); }
    ValueType const& back() const { return values.back(); }

private:
    Derived& asDerived() { return *static_cast<Derived*>(this); }

private:
    friend class ir::BasicBlock;
    friend class ir::Function;
    List<ValueType> values;
};

} // namespace internal

/// Represents a basic block. A basic block is a list of instructions starting
/// with zero or more phi nodes and ending with one terminator instruction.
/// These invariants are not enforced by this class because they may be violated
/// during construction and transformations of the CFG.
class SCATHA_API BasicBlock:
    public Value,
    public internal::CFGList<BasicBlock, Instruction>,
    public NodeWithParent<BasicBlock, Function> {
    friend class internal::CFGList<BasicBlock, Instruction>;
    using ListBase = internal::CFGList<BasicBlock, Instruction>;

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    using PhiIterator      = internal::PhiIteratorImpl<false>;
    using ConstPhiIterator = internal::PhiIteratorImpl<true>;

    explicit BasicBlock(Context& context, std::string name);

    /// Clear operands of all instructions of this basic block. Use this before
    /// removing a (dead) basic block from a function.
    void clearAllOperands() {
        for (auto& inst: *this) {
            inst.clearOperands();
        }
    }

    void eraseAllPhiNodes();

    /// Extract an instruction. Does not clear the operands. Caller takes
    /// ownership of the instruction.
    UniquePtr<Instruction> extract(ConstIterator position) {
        return UniquePtr<Instruction>(values.extract(position));
    }

    /// \overload
    UniquePtr<Instruction> extract(Instruction const* inst) {
        return extract(ConstIterator(inst));
    }

    /// \returns `true` iff this basic block is the entry basic block
    bool isEntry() const;

    /// \returns `true` iff \p inst is an instruction of this basic block.
    /// \warning This is linear in the number of instructions in this basic
    /// block.
    bool contains(Instruction const& inst) const;

    /// \returns the terminator instruction if this basic block is well formed
    /// or nullptr
    TerminatorInst const* terminator() const;

    /// \overload
    TerminatorInst* terminator() {
        return const_cast<TerminatorInst*>(
            static_cast<BasicBlock const*>(this)->terminator());
    }

    /// \returns a view over the phi nodes in this basic block.
    ranges::subrange<ConstPhiIterator, internal::PhiSentinel> phiNodes() const {
        return { ConstPhiIterator{ begin(), end() }, {} };
    }

    /// \overload
    ranges::subrange<PhiIterator, internal::PhiSentinel> phiNodes() {
        return { PhiIterator{ begin(), end() }, {} };
    }

    /// \returns a view over the basic blocks this basic block is directly
    /// reachable from
    std::span<BasicBlock* const> predecessors() { return preds; }

    /// \overload
    std::span<BasicBlock const* const> predecessors() const { return preds; }

    /// Update the predecessor \p oldPred to \p newPred
    ///
    /// \pre \p oldPred must be a predecessor of this basic block.
    ///
    /// This also updates all the phi nodes in this basic block.
    void updatePredecessor(BasicBlock const* oldPred, BasicBlock* newPred);

    /// \returns `true`iff \p *possiblePred is a predecessor of this basic
    /// block.
    bool isPredecessor(BasicBlock const* possiblePred) const {
        return std::find(preds.begin(), preds.end(), possiblePred) !=
               preds.end();
    }

    /// Mark \p *pred as a predecessor of this basic block.
    /// \pre \p *pred must not yet be marked as predecessor.
    void addPredecessor(BasicBlock* pred) {
        SC_ASSERT(!isPredecessor(pred),
                  "This basic block already is a predecessor");
        preds.push_back(pred);
    }

    /// Make \p preds the marked list of predecessors of this basic block.
    /// Caller is responsible that these basic blocks are actually
    /// predecessords.
    void setPredecessors(std::span<BasicBlock* const> newPreds) {
        preds.clear();
        std::copy(newPreds.begin(), newPreds.end(), std::back_inserter(preds));
    }

    /// Remove \p *pred from the list of predecessors of this basic block.
    /// \pre \p *pred must be a listed predecessor of this basic block.
    void removePredecessor(BasicBlock const* pred);

    /// The basic blocks directly reachable from this basic block
    auto successors();

    /// \overload
    auto successors() const;

    /// \returns `true` iff this basic block has exactly one predecessor.
    bool hasSinglePredecessor() const { return preds.size() == 1; }

    /// \returns predecessor if this basic block has a single predecessor, else
    /// `nullptr`.
    BasicBlock* singlePredecessor() {
        return const_cast<BasicBlock*>(
            static_cast<BasicBlock const*>(this)->singlePredecessor());
    }

    /// \overload
    BasicBlock const* singlePredecessor() const {
        return hasSinglePredecessor() ? preds.front() : nullptr;
    }

    /// \returns `true` iff this basic block has exactly one successor.
    bool hasSingleSuccessor() const;

    /// \returns successor if this basic block has a single successor, else
    /// `nullptr`.
    BasicBlock* singleSuccessor() {
        return const_cast<BasicBlock*>(
            static_cast<BasicBlock const*>(this)->singleSuccessor());
    }

    /// \overload
    BasicBlock const* singleSuccessor() const;

private:
    /// For access to insert and erase callbacks.
    friend class Function;

    /// Callbacks used by CFGList base class
    void insertCallback(Instruction&);
    void eraseCallback(Instruction const&);

private:
    utl::small_vector<BasicBlock*> preds;
};

/// Represents a function parameter.
class SCATHA_API Parameter:
    public Value,
    public NodeWithParent<Parameter, Function> {
    using NodeBase = NodeWithParent<Parameter, Function>;

public:
    explicit Parameter(Type const* type, size_t index, Function* parent):
        Value(NodeType::Parameter, type, std::to_string(index)),
        NodeBase(parent),
        _index(index) {}

    /// \returns the index of this parameter which is also its name
    size_t index() const { return _index; }

private:
    size_t _index;
};

/// Represents a function. A function is a prototype with a list of basic
/// blocks.
class SCATHA_API Function:
    public Constant,
    public internal::CFGList<Function, BasicBlock>,
    public NodeWithParent<Function, Module> {
    friend class internal::CFGList<Function, BasicBlock>;
    using ListBase = internal::CFGList<Function, BasicBlock>;

    static auto getParametersImpl(auto&& self) {
        /// Trick to return a view over parameters without returning the
        /// `List<>` itself.
        return self.params | ranges::views::transform(
                                 [](auto& param) -> auto& { return param; });
    }

    template <typename Itr, typename Self>
    static ranges::subrange<Itr> getInstructionsImpl(Self&& self) {
        using InstItr = typename Itr::InstructionIterator;
        Itr const begin(self.values.begin(),
                        self.values.empty() ? InstItr{} :
                                              self.values.front().begin());
        Itr const end(self.values.end(), InstItr{});
        return { begin, end };
    }

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    using InstructionIterator =
        internal::InstructionIteratorImpl<Function::Iterator,
                                          BasicBlock::Iterator>;
    using ConstInstructionIterator =
        internal::InstructionIteratorImpl<Function::ConstIterator,
                                          BasicBlock::ConstIterator>;

    /// Construct a function
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name);

    /// \returns the return type of this function
    Type const* returnType() const { return _returnType; }

    /// \returns a view over the function parameters
    auto parameters() { return getParametersImpl(*this); }

    /// \overload
    auto parameters() const { return getParametersImpl(*this); }

    /// \returns the entry basic block of this function
    BasicBlock& entry() { return front(); }

    /// \overload
    BasicBlock const& entry() const { return front(); }

    /// \returns a view over all instructions in this `Function`.
    ranges::subrange<InstructionIterator> instructions() {
        return getInstructionsImpl<InstructionIterator>(*this);
    }

    /// \overload
    auto instructions() const {
        return getInstructionsImpl<ConstInstructionIterator>(*this);
    }

private:
    /// Callbacks used by CFGList base class
    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

private:
    /// For access to `nameFac`
    friend class Value;
    friend class BasicBlock;

    Type const* _returnType;
    List<Parameter> params;
    UniqueNameFactory nameFac;
};

/// Outlined because `Function` is incomplete at definition of `BasicBlock`
inline bool BasicBlock::isEntry() const {
    return parent()->begin().to_address() == this;
}

/// `alloca` instruction. Allocates automatically managed memory for local
/// variables. Its value is a pointer to the allocated memory.
class SCATHA_API Alloca: public Instruction {
public:
    explicit Alloca(Context& context,
                    Type const* allocatedType,
                    std::string name);

    /// \returns the type allocated by this `alloca` instruction
    Type const* allocatedType() const { return typeOperands()[0]; }
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

    /// \returns the type of the operand of this instruction
    Type const* operandType() const { return operand()->type(); }
};

/// `load` instruction. Load data from memory into a register.
class SCATHA_API Load: public UnaryInstruction {
public:
    explicit Load(Value* address, Type const* type, std::string name):
        UnaryInstruction(NodeType::Load, address, type, std::move(name)) {
        SC_ASSERT(isa<PointerType>(address->type()),
                  "`address` must be of type `ptr`");
    }

    /// \returns the address this instruction loads from
    Value* address() { return operand(); }

    /// \overload
    Value const* address() const { return operand(); }
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

    /// \returns the RHS operand
    Value* rhs() { return operands()[1]; }

    ///  \overload
    Value const* rhs() const { return operands()[1]; }

    /// \returns the type of the operands.
    Type const* operandType() const { return lhs()->type(); }
};

/// `cmp` instruction.
/// TODO: Rename to 'Compare' or find a uniform naming scheme across the IR
/// module.
class SCATHA_API CompareInst: public BinaryInstruction {
public:
    explicit CompareInst(Context& context,
                         Value* lhs,
                         Value* rhs,
                         CompareOperation op,
                         std::string name);

    CompareOperation operation() const { return _op; }

private:
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
               ranges::views::transform([](auto* value) {
                   return value ? cast<T*>(value) : nullptr;
               });
    }

public:
    auto targets() { return targetsImpl<BasicBlock>(*this); }

    auto targets() const { return targetsImpl<BasicBlock const>(*this); }

    void updateTarget(BasicBlock const* oldTarget, BasicBlock* newTarget) {
        updateOperand(oldTarget, newTarget);
    }

    void setTarget(size_t index, BasicBlock* bb) {
        setOperand(nonTargetArguments + index, bb);
    }

protected:
    explicit TerminatorInst(NodeType nodeType,
                            Context& context,
                            std::initializer_list<Value*> operands,
                            std::initializer_list<BasicBlock*> targets);

private:
    uint16_t nonTargetArguments = 0;
};

/// Now that we have defined `TerminatorInst` we can define
/// `BasicBlock::successors()`
namespace internal {

static auto succImpl(auto* t) {
    SC_ASSERT(t, "No successors without a terminator");
    return t->targets();
}

} // namespace internal

inline auto BasicBlock::successors() {
    return internal::succImpl(terminator());
}

inline auto BasicBlock::successors() const {
    return internal::succImpl(terminator());
}

inline bool BasicBlock::hasSingleSuccessor() const {
    return successors().size() == 1;
}

inline BasicBlock const* BasicBlock::singleSuccessor() const {
    return hasSingleSuccessor() ? successors().front() : nullptr;
}

/// `goto` instruction. Leave the current basic block and unconditionally enter
/// the target basic block.
class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target):
        TerminatorInst(NodeType::Goto, context, {}, { target }) {}

    BasicBlock* target() { return targets()[0]; }
    BasicBlock const* target() const { return targets()[0]; }

    void setTarget(BasicBlock* bb) { setOperand(0, bb); }

    using TerminatorInst::setTarget;
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
                       { thenTarget, elseTarget }) {
        SC_ASSERT(cast<IntegralType const*>(condition->type())->bitWidth() == 1,
                  "Condition must be of type i1");
    }

    Value* condition() { return operands()[0]; }
    Value const* condition() const { return operands()[0]; }

    BasicBlock* thenTarget() { return targets()[0]; }
    BasicBlock const* thenTarget() const { return targets()[0]; }

    BasicBlock* elseTarget() { return targets()[1]; }
    BasicBlock const* elseTarget() const { return targets()[1]; }

    void setThenTarget(BasicBlock* bb) { setOperand(1, bb); }

    void setElseTarget(BasicBlock* bb) { setOperand(2, bb); }
};

/// `return` instruction. Return control flow to the calling function.
///
/// \details
class SCATHA_API Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value):
        TerminatorInst(NodeType::Return, context, { value }, {}) {
        SC_ASSERT(value != nullptr, "We don't want null operands");
    }

    explicit Return(Context& context);

    /// May be null in case of a void function
    Value* value() {
        return const_cast<Value*>(static_cast<Return const*>(this)->value());
    }

    /// Value returned by this return instruction.
    /// If the parent function returns void, this is an unspecified value of
    /// type void.
    Value const* value() const { return operands()[0]; }
};

/// `call` instruction. Calls a function.
///
/// \details
/// Callee is stored as the first operand. Arguments are the following operands
/// starting from index 1.
class SCATHA_API FunctionCall: public Instruction {
public:
    explicit FunctionCall(Function* function,
                          std::span<Value* const> arguments,
                          std::string name);

    Function* function() { return cast<Function*>(operands()[0]); }
    Function const* function() const {
        return cast<Function const*>(operands()[0]);
    }

    auto arguments() { return operands() | ranges::views::drop(1); }
    auto arguments() const { return operands() | ranges::views::drop(1); }
};

/// `ext call` instruction. Calls an external function.
///
/// \details
/// Call arguments are the operands. Callee information is not visible to
/// `Instruction` base class, as it's just indices.
class SCATHA_API ExtFunctionCall: public Instruction {
public:
    explicit ExtFunctionCall(size_t slot,
                             size_t index,
                             std::string functionName,
                             std::span<Value* const> arguments,
                             ir::Type const* returnType,
                             std::string name);

    /// Slot in external function table of VM.
    size_t slot() const { return _slot; }

    /// Index into slot.
    size_t index() const { return _index; }

    /// Name of the called function.
    std::string_view functionName() const { return _functionName; }

    /// Arguments for this call.
    std::span<Value* const> arguments() { return operands(); }

    /// \overload
    std::span<Value const* const> arguments() const { return operands(); }

private:
    u32 _slot, _index;
    std::string _functionName;
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

    /// Number of arguments. Must match the number of predecessors of parent
    /// basic block.
    size_t argumentCount() const { return _preds.size(); }

    /// Access argument pair at index \p index
    PhiMapping argumentAt(size_t index) {
        SC_ASSERT(index < argumentCount(), "");
        return { _preds[index], operands()[index] };
    }

    /// \overload
    ConstPhiMapping argumentAt(size_t index) const {
        SC_ASSERT(index < argumentCount(), "");
        return { _preds[index], operands()[index] };
    }

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

/// `gep` or GetElementPointer instruction. Calculate offset pointer to a
/// structure member or array element.
class GetElementPointer: public Instruction {
public:
    explicit GetElementPointer(Context& context,
                               Type const* accessedType,
                               Value* basePointer,
                               Value* arrayIndex,
                               std::initializer_list<size_t> memberIndices,
                               std::string name):
        GetElementPointer(context,
                          accessedType,
                          basePointer,
                          arrayIndex,
                          std::span<size_t const>(memberIndices),
                          name) {}

    explicit GetElementPointer(Context& context,
                               Type const* accessedType,
                               Value* basePointer,
                               Value* arrayIndex,
                               std::span<size_t const> memberIndices,
                               std::string name);

    Type const* accessedType() const { return typeOperands()[0]; }

    Value* basePointer() { return operands()[0]; }
    Value const* basePointer() const { return operands()[0]; }

    Value* arrayIndex() { return operands()[1]; }
    Value const* arrayIndex() const { return operands()[1]; }

    std::span<uint16_t const> memberIndices() const { return _memberIndices; }

    bool hasConstantArrayIndex() const {
        return isa<IntegralConstant>(arrayIndex());
    }

    size_t constantArrayIndex() const {
        return cast<IntegralConstant const*>(arrayIndex())
            ->value()
            .to<size_t>();
    }

private:
    utl::small_vector<uint16_t> _memberIndices;
};

namespace internal {

class AccessValueBase {
public:
    explicit AccessValueBase(std::span<size_t const> indices):
        _indices(indices | ranges::to<utl::small_vector<uint16_t>>) {}

    std::span<uint16_t const> memberIndices() const { return _indices; }

protected:
    static Type const* computeAccessedType(Type const* operandType,
                                           std::span<size_t const> indices);

private:
    utl::small_vector<uint16_t> _indices;
};

} // namespace internal

/// `extract_value` instruction. Extract the value of a structure member or
/// array element.
class ExtractValue: public UnaryInstruction, public internal::AccessValueBase {
public:
    explicit ExtractValue(Value* baseValue,
                          std::initializer_list<size_t> indices,
                          std::string name):
        ExtractValue(baseValue,
                     std::span<size_t const>(indices),
                     std::move(name)) {}

    explicit ExtractValue(Value* baseValue,
                          std::span<size_t const> indices,
                          std::string name):
        UnaryInstruction(NodeType::ExtractValue,
                         baseValue,
                         computeAccessedType(baseValue->type(), indices),
                         std::move(name)),
        internal::AccessValueBase(indices) {}

    /// The structure or array being accessed. Same as `operand()`
    Value* baseValue() { return operand(); }

    /// \overload
    Value const* baseValue() const { return operand(); }
};

/// `insert_value` instruction. Insert a value into a structure or array.
class InsertValue: public BinaryInstruction, public internal::AccessValueBase {
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
                         std::string name):
        BinaryInstruction(NodeType::InsertValue,
                          baseValue,
                          insertedValue,
                          baseValue->type(),
                          std::move(name)),
        internal::AccessValueBase(indices) {
        SC_ASSERT(insertedValue->type() ==
                      computeAccessedType(baseValue->type(), indices),
                  "Type mismatch");
    }

    /// The structure or array being accessed. Same as `lhs()`
    Value* baseValue() { return lhs(); }

    /// \overload
    Value const* baseValue() const { return lhs(); }

    /// The value being inserted. Same as `rhs()`
    Value* insertedValue() { return rhs(); }

    /// \overload
    Value const* insertedValue() const { return rhs(); }
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_H_
