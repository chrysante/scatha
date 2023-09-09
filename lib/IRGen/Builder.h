#ifndef SCATHA_IRGEN_BUILDER_H_
#define SCATHA_IRGEN_BUILDER_H_

#include <string>

#include <utl/vector.hpp>

#include "IR/Fwd.h"

namespace scatha::irgen {

/// Helper class to build IR
class Builder {
public:
    explicit Builder(ir::Context& ctx, ir::Function& function);

    /// Access the currently active basic block, i.e. the block that was added
    /// last to the function
    ir::BasicBlock& currentBlock() const { return *currentBB; }

    /// Creates a new basic block with name \p name without adding it to the
    /// current function
    ir::BasicBlock* newBlock(std::string name);

    /// Add the basic block \p BB to the current function
    /// \Returns the argument
    ir::BasicBlock* add(ir::BasicBlock* BB);

    /// Create a new basic block with name \p name and add it to the current
    /// function
    ir::BasicBlock* addNewBlock(std::string name);

    /// Add the instruction \p inst to the current basic block
    /// \Returns the argument
    ir::Instruction* add(ir::Instruction* inst);

    /// Allocate `Inst` with arguments `...args` and add it to the function
    template <std::derived_from<ir::Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Args...>
    Inst* add(Args&&... args) {
        Inst* result = new Inst(std::forward<Args>(args)...);
        add(result);
        return result;
    }

    /// Allocate `Inst` with arguments `context, ...args` and add it to the
    /// function
    template <std::derived_from<ir::Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, ir::Context&, Args...>
    Inst* add(Args&&... args) {
        auto* result = new Inst(ctx, std::forward<Args>(args)...);
        add(result);
        return result;
    }

    /// Allocates stack memory for a value of type \p type with name \p name
    ir::Value* makeLocalVariable(ir::Type const* type, std::string name);

    /// Allocate stack space for \p value and emit a store instruction storing
    /// \p value into the allocated memory. \Returns a pointer to the allocated
    /// memory region
    ir::Value* storeToMemory(ir::Value* value);

    /// \overload specifying a name for the allocated memory
    ir::Value* storeToMemory(ir::Value* value, std::string name);

private:
    ir::Context& ctx;
    ir::Function& function;
    ir::BasicBlock* currentBB;
    utl::small_vector<ir::Alloca*> allocas;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_BUILDER_H_
