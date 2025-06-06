#ifndef SCATHA_IR_BUILDER_H_
#define SCATHA_IR_BUILDER_H_

#include <concepts>
#include <span>
#include <string>

#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "Common/UniquePtr.h"
#include "IR/CFG/BasicBlock.h"
#include "IR/Fwd.h"

namespace scatha::ir {

///
class BasicBlockBuilder {
public:
    explicit BasicBlockBuilder(Context& ctx, BasicBlock* BB);
    explicit BasicBlockBuilder(Context& ctx, BasicBlock* BB,
                               BasicBlock::ConstIterator addPoint);
    explicit BasicBlockBuilder(Context& ctx, BasicBlock* BB,
                               Instruction const* addPoint);

    /// Adds the instruction \p inst before the currently set 'add point' of the
    /// basic block. By default this is the end iterator.
    /// \Returns the argument
    Instruction* add(Instruction* inst);

    /// Insert the instruction \p inst to the basic block before \p before
    /// \Returns the argument
    Instruction* insert(Instruction const* before, Instruction* inst);

    /// Allocate `Inst` with arguments `...args` and add it to the back of the
    /// basic block \Returns the allocated instruction
    template <std::derived_from<Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Args...>
    Inst* add(Args&&... args) {
        return cast<Inst*>(add(new Inst(std::forward<Args>(args)...)));
    }

    /// Allocate `Inst` with arguments `context, ...args` and add it to the back
    /// of the basic block \Returns the allocated instruction
    template <std::derived_from<Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Context&, Args...>
    Inst* add(Args&&... args) {
        return cast<Inst*>(add(new Inst(ctx, std::forward<Args>(args)...)));
    }

    /// Allocate `Inst` with arguments `...args` and insert it to the basic
    /// block before \p before \Returns the allocated instruction
    template <std::derived_from<Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Args...>
    Inst* insert(Instruction const* before, Args&&... args) {
        return cast<Inst*>(
            insert(before, new Inst(std::forward<Args>(args)...)));
    }

    /// Allocate `Inst` with arguments `context, ...args` and insert it to the
    /// basic block before \p before \Returns the allocated instruction
    template <std::derived_from<Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Context&, Args...>
    Inst* insert(Instruction const* before, Args&&... args) {
        return cast<Inst*>(
            insert(before, new Inst(ctx, std::forward<Args>(args)...)));
    }

    /// Build a structure with repeated `InsertValue` instructions
    /// The elements in \p members must match the struct members exactly
    Value* buildStructure(StructType const* type,
                          std::span<Value* const> members, std::string name);

    /// If \p elems has one value, that value is returned unchanged
    /// If \p elems has more than one values, this function returns
    /// `buildStructure(/*type*/, elems, name)` where `/*type*/` is the
    /// anonymous struct type with the element types of \p elems
    ///
    /// \pre \p elems must not be empty
    Value* packValues(std::span<Value* const> elems, std::string name);

    /// If \p values has one element, that values is returned.
    /// Otherwise generates a binary left fold over \p op of the arguments \p
    /// values and returns the result \pre \p values must not be empty
    ir::Value* foldValues(ir::ArithmeticOperation op,
                          std::span<Value* const> values, std::string name);

    /// Sets the 'add point' to \p newAddPoint
    /// The add point is the iterator before which the `add()` methods insert
    /// instructions
    void setAddPoint(BasicBlock::ConstIterator newAddPoint) {
        instAddPoint = newAddPoint;
    }

    /// \overload
    void setAddPoint(ir::Instruction const* before) {
        setAddPoint(BasicBlock::ConstIterator(before));
    }

    /// \Returns the constant of type \p type with value all bits set to zero
    ir::Constant* makeZeroConstant(ir::Type const* type);

private:
    friend class FunctionBuilder;
    Context& ctx;
    BasicBlock* currentBB;
    BasicBlock::ConstIterator instAddPoint;
};

/// Helper class to build IR functions
class FunctionBuilder: public BasicBlockBuilder {
public:
    explicit FunctionBuilder(Context& ctx, Function* function);

    /// Defaulted in the implemention file because of UniquePtrs member
    ~FunctionBuilder();

    /// Bring `add()` function for instructions into scope
    using BasicBlockBuilder::add;

    /// Access the currently active basic block, i.e. the block that was added
    /// last to the function
    BasicBlock& currentBlock() const { return *currentBB; }

    ///
    void makeBlockCurrent(BasicBlock* BB) { makeBlockCurrent(BB, BB->end()); }

    ///
    void makeBlockCurrent(BasicBlock* BB, BasicBlock::ConstIterator addPoint) {
        currentBB = BB;
        instAddPoint = addPoint;
    }

    ///
    template <std::invocable F>
    decltype(auto) withBlockCurrent(BasicBlock* BB, F&& f) {
        return withBlockCurrent(BB, BB->end(), std::forward<F>(f));
    }

    ///
    template <std::invocable F>
    decltype(auto) withBlockCurrent(BasicBlock* BB,
                                    BasicBlock::ConstIterator addPoint, F&& f) {
        auto* stashed = &currentBlock();
        auto stashedAddPoint = instAddPoint;
        utl::scope_guard guard(
            [&] { makeBlockCurrent(stashed, stashedAddPoint); });
        makeBlockCurrent(BB, addPoint);
        return std::invoke(std::forward<F>(f));
    }

    /// Creates a new basic block with name \p name without adding it to the
    /// current function
    BasicBlock* newBlock(std::string name);

    /// Add the basic block \p BB to the current function
    /// \Returns the argument
    BasicBlock* add(BasicBlock* BB);

    /// Create a new basic block with name \p name and add it to the current
    /// function
    BasicBlock* addNewBlock(std::string name);

    /// Allocates stack memory for a value of type \p type with name \p name
    Alloca* makeLocalVariable(Type const* type, std::string name);

    /// Allocates a local array with possibly dynamic count
    ///
    /// Inserts a `%name = alloca, <element-type>, <count-type> %count`
    /// instruction
    Alloca* makeLocalArray(Type const* elementType, Value* count,
                           std::string name);

    /// \overload for static count
    Alloca* makeLocalArray(Type const* elementType, size_t count,
                           std::string name);

    /// Allocate stack space for \p value and emit a store instruction storing
    /// \p value into the allocated memory. \Returns a pointer to the allocated
    /// memory region
    Alloca* storeToMemory(Value* value);

    /// \overload specifying a name for the allocated memory
    Alloca* storeToMemory(Value* value, std::string name);

    /// Finish construction of the function by inserting all alloca instruction
    /// into the entry block
    void insertAllocas();

private:
    Function& function;
    utl::small_vector<UniquePtr<Alloca>> allocas;
};

} // namespace scatha::ir

#endif // SCATHA_IR_BUILDER_H_
