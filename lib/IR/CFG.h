#ifndef SCATHA_IR_CFG_H_
#define SCATHA_IR_CFG_H_

#include <string>

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
    auto users() const {
        return ranges::views::transform(_users, [](auto&& p) { return p.first; });
    }

    /// Number of users using this value. Multiple uses by the same user are counted as one.
    size_t userCount() const { return _users.size(); }

private:
    friend class User;

    /// Register a user of this value. Won't affect \p user
    void addUserWeak(User* user);

    /// Unregister a user of this value. `this` will _not_ be cleared from the operand list of \p user
    void removeUserWeak(User* user);

    template <typename>
    friend class scatha::UniquePtr;

    template <typename>
    friend class ir::DynAllocator;

    void privateDestroy();
    
    void privateDelete();

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

    void setOperands(utl::small_vector<Value*> operands);

    void removeOperand(size_t index);

    /// This should proably at some point be replaced by some sort of `delete` operation
    void clearOperands();

protected:
    explicit User(NodeType nodeType, Type const* type, std::string name, std::initializer_list<Value*> operands):
        User(nodeType, type, std::move(name), utl::small_vector<Value*>(operands)) {}

    explicit User(NodeType nodeType, Type const* type, std::string name = {}, utl::small_vector<Value*> operands = {});

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

/// Base class of all instructions. Every instruction inherits from `Value` as it (usually) yields a value. If an
/// instruction does not yield a value its `Value` super class is of type void.
class SCATHA(API) Instruction: public User, public NodeWithParent<Instruction, BasicBlock> {
public:
    /// View of all instructions using this value. This casts the elements in the range returned by `Value::users()` to
    /// instructions, as instructions are only used by other instructions.
    auto users() const {
        return ranges::views::transform(Value::users(), []<typename T>(T* user) {
            return cast<utl::copy_cv_t<T, Instruction>*>(user);
        });
    }
    
protected:
    using User::User;
};

/// Represents a basic block. A basic block  is a list of instructions starting with zero or more phi nodes and
/// ending with one terminator instruction. These invariants are not enforced by this class because they may be
/// violated during construction and transformations of the CFG.
class SCATHA(API) BasicBlock: public Value, public NodeWithParent<BasicBlock, Function> {
public:
    using Iterator      = List<Instruction>::iterator;
    using ConstIterator = List<Instruction>::const_iterator;

    using PhiIterator      = internal::PhiIteratorImpl<false>;
    using ConstPhiIterator = internal::PhiIteratorImpl<true>;

    explicit BasicBlock(Context& context, std::string name);

    /// Insert an instruction into this basic block. Callee takes ownership.
    void pushFront(Instruction* instruction) { insert(instructions.begin(), instruction); }

    /// Insert an instruction into this basic block. Callee takes ownership.
    void pushBack(Instruction* instruction) { insert(instructions.end(), instruction); }

    /// Insert an instruction into this basic block. Callee takes ownership.
    void insert(ConstIterator before, Instruction* instruction) {
        instruction->set_parent(this);
        instructions.insert(before, instruction);
    }
    
    /// \overload
    void insert(Instruction const* before, Instruction* instruction) {
        insert(ConstIterator(before), instruction);
    }

    /// Merge `*this` with \p *rhs
    /// Insert nodes of \p *rhs before \p pos
    void splice(ConstIterator pos, BasicBlock* rhs) {
        for (auto& inst: *rhs) {
            inst.set_parent(this);
        }
        instructions.splice(pos, rhs->instructions);
    }

    /// Clear operands of all instructions of this basic block. Use this before removing a (dead) basic block from a
    /// function.
    void clearAllOperands() {
        for (auto& inst: *this) {
            inst.clearOperands();
        }
    }

    /// Erase an instruction. Clears the operands.
    Iterator erase(ConstIterator position) {
        const_cast<Instruction*>(position.to_address())->clearOperands();
        return instructions.erase(position);
    }

    /// \overload
    Iterator erase(Instruction const* inst) { return erase(ConstIterator(inst)); }

    /// \overload
    Iterator erase(ConstIterator first, ConstIterator last) {
        for (Iterator i(const_cast<Instruction*>(first.to_address())); i != last; ++i) {
            i->clearOperands();
        }
        return instructions.erase(first, last);
    }

    void eraseAllPhiNodes() {
        auto phiEnd = begin();
        while (isa<Phi>(*phiEnd)) {
            ++phiEnd;
        }
        erase(begin(), phiEnd);
    }

    /// Extract an instruction. Does not clear the operands. Caller takes ownership of the instruction.
    Instruction* extract(ConstIterator position) { return instructions.extract(position); }

    /// \overload
    Instruction* extract(Instruction const* inst) { return extract(ConstIterator(inst)); }

    /// Check wether this is the entry basic block of a function
    bool isEntry() const;

    /// Check wether \p inst is an instruction of this basic block.
    /// \warning This is linear in the number of instructions in this basic block.
    bool contains(Instruction const& inst) const;

    /// Returns the terminator instruction if this basic block is well formed or nullptr
    TerminatorInst const* terminator() const;

    /// \overload
    TerminatorInst* terminator() {
        return const_cast<TerminatorInst*>(static_cast<BasicBlock const*>(this)->terminator());
    }

    Iterator begin() { return instructions.begin(); }
    ConstIterator begin() const { return instructions.begin(); }
    
    auto rbegin() { return instructions.rbegin(); }
    auto rbegin() const { return instructions.rbegin(); }

    Iterator end() { return instructions.end(); }
    ConstIterator end() const { return instructions.end(); }
    
    auto rend() { return instructions.rend(); }
    auto rend() const { return instructions.rend(); }

    bool empty() const { return instructions.empty(); }

    Instruction& front() { return instructions.front(); }
    Instruction const& front() const { return instructions.front(); }

    Instruction& back() { return instructions.back(); }
    Instruction const& back() const { return instructions.back(); }

    /// View over the phi nodes in this basic block.
    ranges::subrange<ConstPhiIterator, internal::PhiSentinel> phiNodes() const {
        return { ConstPhiIterator{ instructions.begin(), instructions.end() }, {} };
    }

    /// \overload
    ranges::subrange<PhiIterator, internal::PhiSentinel> phiNodes() {
        return { PhiIterator{ instructions.begin(), instructions.end() }, {} };
    }

    /// The basic blocks this basic block is directly reachable from
    std::span<BasicBlock* const> predecessors() { return preds; }

    /// \overload
    std::span<BasicBlock const* const> predecessors() const { return preds; }

    /// Test wether \p *possiblePred is a predecessor of this basic block.
    bool isPredecessor(BasicBlock const* possiblePred) const {
        return std::find(preds.begin(), preds.end(), possiblePred) != preds.end();
    }

    /// Mark \p *pred as a predecessor of this basic block.
    /// \pre \p *pred must not yet be marked as predecessor.
    void addPredecessor(BasicBlock* pred) {
        SC_ASSERT(!isPredecessor(pred), "This basic block already is a predecessor");
        preds.push_back(pred);
    }

    /// Make \p preds the marked list of predecessors of this basic block. Caller is responsible that these basic blocks
    /// are actually predecessords.
    void setPredecessors(std::span<BasicBlock* const> newPreds) {
        preds.clear();
        std::copy(newPreds.begin(), newPreds.end(), std::back_inserter(preds));
    }

    /// Remove \p *pred from the list of predecessors of this basic block.
    /// \pre \p *pred must be a listed predecessor of this basic block.
    void removePredecessor(BasicBlock const* pred) { preds.erase(std::find(preds.begin(), preds.end(), pred)); }

    /// The basic blocks directly reachable from this basic block
    std::span<BasicBlock* const> successors();

    /// \overload
    std::span<BasicBlock const* const> successors() const;

    /// Returns `true` iff this basic block has exactly one predecessor.
    bool hasSinglePredecessor() const { return preds.size() == 1; }

    /// Return predecessor if this basic block has a single predecessor, else `nullptr`.
    BasicBlock* singlePredecessor() {
        return const_cast<BasicBlock*>(static_cast<BasicBlock const*>(this)->singlePredecessor());
    }

    /// \overload
    BasicBlock const* singlePredecessor() const { return hasSinglePredecessor() ? preds.front() : nullptr; }

    /// Returns `true` iff this basic block has exactly one successor.
    bool hasSingleSuccessor() const { return successors().size() == 1; }

    /// Return successor if this basic block has a single successor, else `nullptr`.
    BasicBlock* singleSuccessor() {
        return const_cast<BasicBlock*>(static_cast<BasicBlock const*>(this)->singleSuccessor());
    }

    /// \overload
    BasicBlock const* singleSuccessor() const { return hasSingleSuccessor() ? successors().front() : nullptr; }

private:
    List<Instruction> instructions;
    utl::small_vector<BasicBlock*> preds;
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
    using InstructionIterator =
        internal::InstructionIteratorImpl<List<BasicBlock>::iterator, List<Instruction>::iterator>;
    using ConstInstructionIterator =
        internal::InstructionIteratorImpl<List<BasicBlock>::const_iterator, List<Instruction>::const_iterator>;

    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name);

    Type const* returnType() const { return _returnType; }

    List<Parameter>& parameters() { return params; }
    List<Parameter> const& parameters() const { return params; }

    List<BasicBlock>& basicBlocks() { return bbs; }
    List<BasicBlock> const& basicBlocks() const { return bbs; }

    BasicBlock& entry() { return bbs.front(); }
    BasicBlock const& entry() const { return bbs.front(); }

    ranges::subrange<InstructionIterator> instructions() { return getInstructionsImpl<InstructionIterator>(*this); }
    auto instructions() const { return getInstructionsImpl<ConstInstructionIterator>(*this); }

    void addBasicBlock(BasicBlock* basicBlock) {
        basicBlock->set_parent(this);
        bbs.push_back(basicBlock);
    }

private:
    template <typename Itr, typename Self>
    static ranges::subrange<Itr> getInstructionsImpl(Self&& self) {
        using InstItr = typename Itr::InstructionIterator;
        Itr const begin(self.bbs.begin(), self.bbs.empty() ? InstItr{} : self.bbs.front().begin());
        Itr const end(self.bbs.end(), InstItr{});
        return { begin, end };
    }

private:
    Type const* _returnType;
    List<Parameter> params;
    List<BasicBlock> bbs;
};

/// `alloca` instruction. Allocates automatically managed memory for local variables. Its value is a pointer to the
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
public:
    std::span<BasicBlock* const> targets() { return _targets; }
    std::span<BasicBlock const* const> targets() const { return _targets; }

    void updateTarget(BasicBlock const* oldTarget, BasicBlock* newTarget) {
        *std::find(_targets.begin(), _targets.end(), oldTarget) = newTarget;
    }

protected:
    explicit TerminatorInst(NodeType nodeType,
                            Context& context,
                            utl::small_vector<BasicBlock*> targets,
                            std::initializer_list<Value*> operands = {});

    utl::small_vector<BasicBlock*> _targets;
};

/// Goto instruction. Leave the current basic block and unconditionally enter the target basic block.
class Goto: public TerminatorInst {
public:
    explicit Goto(Context& context, BasicBlock* target): TerminatorInst(NodeType::Goto, context, { target }) {}

    BasicBlock* target() { return targets()[0]; }
    BasicBlock const* target() const { return targets()[0]; }
};

/// Branch instruction. Leave the current basic block and choose a target basic block based on a condition.
class SCATHA(API) Branch: public TerminatorInst {
public:
    explicit Branch(Context& context, Value* condition, BasicBlock* thenTarget, BasicBlock* elseTarget):
        TerminatorInst(NodeType::Branch, context, { thenTarget, elseTarget }, { condition }) {
        SC_ASSERT(cast<IntegralType const*>(condition->type())->bitWidth() == 1, "Condition must be of type i1");
    }

    Value* condition() { return operands()[0]; }
    Value const* condition() const { return operands()[0]; }
    BasicBlock* thenTarget() { return targets()[0]; }
    BasicBlock const* thenTarget() const { return targets()[0]; }
    BasicBlock* elseTarget() { return targets()[1]; }
    BasicBlock const* elseTarget() const { return targets()[1]; }
};

/// Return instruction. Return control flow to the calling function.
class SCATHA(API) Return: public TerminatorInst {
public:
    explicit Return(Context& context, Value* value): TerminatorInst(NodeType::Return, context, {}, { value }) {
        SC_ASSERT(value != nullptr, "We don't want null operands");
    }

    explicit Return(Context& context): TerminatorInst(NodeType::Return, context, {}, {}) {}

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
                             std::string functionName,
                             std::span<Value* const> arguments,
                             ir::Type const* returnType,
                             std::string name = {});

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

/// Phi instruction. Choose a value based on where control flow comes from.
class SCATHA(API) Phi: public Instruction {
public:
    /// \overload
    explicit Phi(std::initializer_list<PhiMapping> args, std::string name):
        Phi(std::span<PhiMapping const>(args), std::move(name)) {}

    /// Construct a phi node with a set of arguments.
    explicit Phi(std::span<PhiMapping const> args, std::string name):
        Phi(nullptr /* Type will be set by call to setArguments() */, std::move(name)) {
        setArguments(args);
    }

    /// Construct an empty phi node.
    explicit Phi(Type const* type, std::string name = {}): Instruction(NodeType::Phi, type, std::move(name), {}) {}

    /// Assign arguments to this phi node.
    void setArguments(std::span<PhiMapping const> args);

    /// Number of arguments. Must match the number of predecessors of parent basic block.
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

    /// View over all incoming edges. Must be the same as predecessors of parent basic block.
    std::span<BasicBlock* const> incomingEdges() { return _preds; }

    /// \overload
    std::span<BasicBlock const* const> incomingEdges() const { return _preds; }

    /// View over arguments
    auto arguments() {
        return ranges::views::zip(_preds, operands()) | ranges::views::transform([](auto p) -> PhiMapping { return p; });
    }

    /// \overload
    auto arguments() const {
        return ranges::views::zip(_preds, operands()) | ranges::views::transform([](auto p) -> ConstPhiMapping { return p; });
    }

    size_t indexOf(BasicBlock const* predecessor) const {
        return utl::narrow_cast<size_t>(std::find(_preds.begin(), _preds.end(), predecessor) - _preds.begin());
    }

    /// Remove the argument corresponding to \p *predecessor
    /// \p *predecessor must be an argument of this phi instruction.
    void removeArgument(BasicBlock const* predecessor) { removeArgument(indexOf(predecessor)); }

    /// Remove the argument at index \p index
    void removeArgument(size_t index) {
        _preds.erase(_preds.begin() + index);
        removeOperand(index);
    }

private:
    utl::small_vector<BasicBlock*> _preds;
};

/// GetElementPointer instruction. Calculate offset pointer to a structure member or array element.
class GetElementPointer: public Instruction {
public:
    explicit GetElementPointer(Context& context,
                               Type const* accessedType,
                               Type const* pointeeType,
                               Value* basePointer,
                               Value* arrayIndex,
                               Value* structMemberIndex,
                               std::string name = {});

    Type const* accessedType() const { return accType; }

    Type const* pointeeType() const { return cast<PointerType const*>(type())->pointeeType(); }

    bool isAllConstant() const {
        return isa<IntegralConstant>(arrayIndex()) && isa<IntegralConstant>(structMemberIndex());
    }

    Value* basePointer() { return operands()[0]; }
    Value const* basePointer() const { return operands()[0]; }

    Value* arrayIndex() { return operands()[1]; }
    Value const* arrayIndex() const { return operands()[1]; }

    Value* structMemberIndex() { return operands()[2]; }
    Value const* structMemberIndex() const { return operands()[2]; }

    size_t const constantArrayIndex() const {
        return cast<IntegralConstant const*>(arrayIndex())->value().to<size_t>();
    }

    size_t const constantStructMemberIndex() const {
        return cast<IntegralConstant const*>(structMemberIndex())->value().to<size_t>();
    }

    size_t const constantByteOffset() const {
        size_t result = accessedType()->size() * constantArrayIndex();
        if (auto* structType = dyncast<StructureType const*>(accessedType())) {
            result += structType->memberOffsetAt(constantStructMemberIndex());
        }
        return result;
    }

private:
    Type const* accType;
};

namespace internal {

class AccessValueBase {
public:
    explicit AccessValueBase(Value* index): _index(index) {}

    Value* index() { return _index; }
    Value const* index() const { return _index; }

    bool indexIsConstant() const { return isa<IntegralConstant>(_index); }

    size_t constantIndex() const { return cast<IntegralConstant const*>(_index)->value().to<size_t>(); }

private:
    Value* _index;
};

} // namespace internal

/// ExtractValue instruction. Extract the value of a structure member or array element.
class ExtractValue: public UnaryInstruction, public internal::AccessValueBase {
public:
    explicit ExtractValue(Type const* memberType, Value* baseValue, Value* index, std::string name):
        UnaryInstruction(NodeType::ExtractValue,
                         baseValue,
                         memberType,
                         std::move(name)),
        internal::AccessValueBase(index) {}

    /// The structure or array being accessed. Same as `operand()`
    Value* baseValue() { return operand(); }

    /// \overload
    Value const* baseValue() const { return operand(); }
};

/// InsertValue instruction. Insert a value into a structure or array.
class InsertValue: public BinaryInstruction, public internal::AccessValueBase {
public:
    explicit InsertValue(Value* baseValue, Value* insertedValue, Value* index, std::string name):
        BinaryInstruction(NodeType::InsertValue,
                          baseValue,
                          insertedValue,
                          baseValue->type(),
                          std::move(name)),
        internal::AccessValueBase(index) {}

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
