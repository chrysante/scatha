#ifndef SCATHA_IR_CFG_H_
#define SCATHA_IR_CFG_H_

#include <string>

#include <utl/hash.hpp>
#include <utl/hashmap.hpp>
#include <utl/ranges.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
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
        return utl::transform(_users, [](auto&& p) { return p.first; });
    }

    /// Number of users using this value. Multiple uses by the same user are counted as one.
    size_t userCount() const { return _users.size(); }

private:
    friend class User;

    /// Register a user of this value. Won't affect \p user
    void addUserWeak(User* user);
    
    /// Unregister a user of this value. \p this will _not_ be cleared from the operand list of \p user
    void removeUserWeak(User* user);
    
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

    /// This should proably at some point be replaced by some sort of \p delete operation
    void clearOperands();
    
protected:
    explicit User(NodeType nodeType, Type const* type, std::string name, std::initializer_list<Value*> operands):
        User(nodeType, type, std::move(name), std::span<Value* const>(operands)) {}

    explicit User(NodeType nodeType, Type const* type, std::string name = {}, std::span<Value* const> operands = {});

protected:
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

/// Represents a basic block. A basic block  is a list of instructions starting with zero or more phi nodes and
/// ending with one terminator instruction. These invariants are not enforced by this class because they may be
/// violated during construction and transformations of the CFG.
class SCATHA(API) BasicBlock: public Value, public NodeWithParent<BasicBlock, Function> {
public:
    using Iterator = List<Instruction>::iterator;
    using ConstIterator = List<Instruction>::const_iterator;
    
    using PhiIterator      = internal::PhiIteratorImpl<Iterator>;
    using ConstPhiIterator = internal::PhiIteratorImpl<ConstIterator>;

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

    /// Merge \p this with \p rhs
    /// Insert nodes of \p rhs before \p pos
    void splice(ConstIterator pos, BasicBlock* rhs) {
        instructions.splice(pos, rhs->instructions);
    }
    
    /// Clear operands of all instructions of this basic block. Use this before removing a (dead) basic block from a function.
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
    Iterator erase(Instruction const* inst) {
        return erase(ConstIterator(inst));
    }
    
    /// \overload
    Iterator erase(ConstIterator first, ConstIterator last) {
        for (Iterator i(const_cast<Instruction*>(first.to_address())); i != last; ++i) {
            i->clearOperands();
        }
        return instructions.erase(first, last);
    }
    
    /// Extract an instruction. Does not clear the operands. Caller takes ownership of the instruction.
    Instruction* extract(ConstIterator position) {
        return instructions.extract(position);
    }
    
    /// \overload
    Instruction* extract(Instruction const* inst) {
        return extract(ConstIterator(inst));
    }
    
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
    
    Iterator end() { return instructions.end(); }
    ConstIterator end() const { return instructions.end(); }
    
    bool empty() const { return instructions.empty(); }
    
    Instruction& front() { return instructions.front(); }
    Instruction const& front() const { return instructions.front(); }
    
    Instruction& back() { return instructions.back(); }
    Instruction const& back() const { return instructions.back(); }
    
    /// View over the phi nodes in this basic block.
    utl::range_view<ConstPhiIterator, internal::PhiSentinel> phiNodes() const {
        return { ConstPhiIterator{ instructions.begin(), instructions.end() }, {} };
    }

    /// \overload
    utl::range_view<PhiIterator, internal::PhiSentinel> phiNodes() {
        return { PhiIterator{ instructions.begin(), instructions.end() }, {} };
    }
    
    /// The basic blocks this basic block is directly reachable from
    std::span<BasicBlock* const> predecessors() { return preds; }

    /// \overload
    std::span<BasicBlock const* const> predecessors() const { return preds; }

    /// Test wether \p possiblePred is a predecessor of this basic block.
    bool isPredecessor(BasicBlock const* possiblePred) const {
        return std::find(preds.begin(), preds.end(), possiblePred) != preds.end();
    }
    
    /// Mark \p pred as a predecessor of this basic block.
    /// \pre \p pred must not yet be marked as predecessor.
    void addPredecessor(BasicBlock* pred) {
        SC_ASSERT(!isPredecessor(pred), "This basic block already is a predecessor");
        preds.push_back(pred);
    }
    
    /// Make \p preds the marked list of predecessors of this basic block. Caller is responsible that these basic blocks are actually predecessords.
    void setPredecessors(std::span<BasicBlock* const> newPreds) {
        preds.clear();
        std::copy(newPreds.begin(), newPreds.end(), std::back_inserter(preds));
    }
    
    /// Remove \p pred from the list of predecessors of this basic block.
    /// \pre \p pred must be a listed predecessor of this basic block.
    void removePredecessor(BasicBlock const* pred) {
        preds.erase(std::find(preds.begin(), preds.end(), pred));
    }
    
    /// The basic blocks directly reachable from this basic block
    std::span<BasicBlock* const> successors();

    /// \overload
    std::span<BasicBlock const* const> successors() const;

    /// Returns true iff this basic block has exactly one predecessor.
    bool hasSinglePredecessor() const {
        return preds.size() == 1;
    }
    
    /// Return predecessor if this basic block has a single predecessor, else nullptr.
    BasicBlock* singlePredecessor() {
        return const_cast<BasicBlock*>(static_cast<BasicBlock const*>(this)->singlePredecessor());
    }
    
    /// \overload
    BasicBlock const* singlePredecessor() const {
        return hasSinglePredecessor() ? preds.front() : nullptr;
    }
    
    /// Returns true iff this basic block has exactly one successor.
    bool hasSingleSuccessor() const {
        return successors().size() == 1;
    }
    
    /// Return successor if this basic block has a single successor, else nullptr.
    BasicBlock* singleSuccessor() {
        return const_cast<BasicBlock*>(static_cast<BasicBlock const*>(this)->singleSuccessor());
    }
    
    /// \overload
    BasicBlock const* singleSuccessor() const {
        return hasSingleSuccessor() ? successors().front() : nullptr;
    }
    
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

    utl::range_view<InstructionIterator> instructions() { return getInstructionsImpl<InstructionIterator>(*this); }
    utl::range_view<ConstInstructionIterator> instructions() const {
        return getInstructionsImpl<ConstInstructionIterator>(*this);
    }

    void addBasicBlock(BasicBlock* basicBlock) {
        basicBlock->set_parent(this);
        bbs.push_back(basicBlock);
    }

private:
    template <typename Itr, typename Self>
    static utl::range_view<Itr> getInstructionsImpl(Self&& self) {
        using InstItr = typename Itr::InstructionIterator;
        Itr const begin(self.bbs.begin(), self.bbs.empty() ? InstItr{} : self.bbs.front().begin());
        Itr const end(self.bbs.end(), InstItr{});
        return utl::range_view<Itr>{ begin, end };
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
public:
    std::span<BasicBlock* const> targets() { return _targets; }
    std::span<BasicBlock const* const> targets() const { return _targets; }

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
    explicit Return(Context& context, Value* value = nullptr):
        TerminatorInst(NodeType::Return, context, {}, { value }) {}

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

    std::span<BasicBlock* const> incomingEdges() { return _preds; }
    std::span<BasicBlock const* const> incomingEdges() const { return _preds; }

    auto arguments() {
        return utl::transform(utl::iota(argumentCount()), [this](size_t index) { return argumentAt(index); });
    }
    auto arguments() const {
        return utl::transform(utl::iota(argumentCount()), [this](size_t index) { return argumentAt(index); });
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
        return static_cast<u64>(cast<IntegralConstant const*>(arrayIndex())->value());
    }

    size_t const constantStructMemberIndex() const {
        return static_cast<u64>(cast<IntegralConstant const*>(structMemberIndex())->value());
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

/// ExtractValue instruction. Extract the value of a structure member or array element.
class ExtractValue: public UnaryInstruction {
public:
    explicit ExtractValue(Value* baseValue, size_t index, std::string name = {}):
        UnaryInstruction(NodeType::ExtractValue,
                         baseValue,
                         cast<StructureType const*>(baseValue->type())->memberAt(index),
                         std::move(name)) {}

    size_t index() const { return _index; }

private:
    size_t _index;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_H_
