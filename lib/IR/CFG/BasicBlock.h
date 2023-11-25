#ifndef SCATHA_IR_CFG_BASICBLOCK_H_
#define SCATHA_IR_CFG_BASICBLOCK_H_

#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "IR/CFG/Instruction.h"
#include "IR/CFG/Iterator.h"
#include "IR/CFG/Value.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a basic block. A basic block is a list of instructions starting
/// with zero or more phi nodes and ending with one terminator instruction.
/// These invariants are not enforced by this class because they may be violated
/// during construction and transformations of the CFG.
class SCATHA_API BasicBlock:
    public Value,
    public CFGList<BasicBlock, Instruction>,
    public ListNode<BasicBlock>,
    public ParentedNode<Function> {
    friend class CFGList<BasicBlock, Instruction>;
    using ListBase = CFGList<BasicBlock, Instruction>;

    static auto succImpl(auto* t) {
        SC_ASSERT(t, "No successors without a terminator");
        return t->targets();
    }

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    using PhiIterator = internal::PhiIteratorImpl<false>;
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

    /// \returns `true` if this basic block is the entry basic block.
    bool isEntry() const;

    /// \returns `true` if \p inst is an instruction of this basic block.
    /// \warning This is linear in the number of instructions in this basic
    /// block.
    bool contains(Instruction const& inst) const;

    /// \returns the terminator instruction if this basic block is well formed
    /// or `nullptr`.
    template <typename TermInst = TerminatorInst>
    TermInst const* terminator() const {
        if (empty()) {
            return nullptr;
        }
        return dyncast<TermInst const*>(&back());
    }

    /// \overload
    template <typename TermInst = TerminatorInst>
    TermInst* terminator() {
        return const_cast<TermInst*>(
            static_cast<BasicBlock const*>(this)->terminator());
    }

    /// \returns `true` if the terminator is the only instruction in the basic
    /// block.
    bool emptyExceptTerminator() const;

    /// \returns a view over the phi nodes in this basic block.
    ranges::subrange<ConstPhiIterator, internal::PhiSentinel> phiNodes() const {
        return { ConstPhiIterator{ begin(), end() }, {} };
    }

    /// \overload
    ranges::subrange<PhiIterator, internal::PhiSentinel> phiNodes() {
        return { PhiIterator{ begin(), end() }, {} };
    }

    /// \Returns Iterator to the first non-phi instruction in this basic block.
    Iterator phiEnd();

    /// \overload
    ConstIterator phiEnd() const;

    /// \Returns `true` if this block has any phi nodes
    bool hasPhiNodes() const;

    /// Insert a new phi node after the last current phi node
    void insertPhi(Phi* phiNode);

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

    /// Update all predecessors according to \p operation
    void mapPredecessors(
        utl::function_view<BasicBlock*(BasicBlock*)> operation);

    /// \returns `true` if \p *possiblePred is a predecessor of this basic
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

    /// Remove predecessor \p *pred from the list of predecessors of this basic
    /// block. All the phi instructions in this block are updated.
    /// \pre \p *pred must be a listed predecessor of this basic block.
    void removePredecessor(BasicBlock const* pred);

    /// \overload
    void removePredecessor(size_t index);

    /// The basic blocks directly reachable from this basic block
    template <typename TermInst = TerminatorInst>
    auto successors() {
        return succImpl(terminator<TermInst>());
    }

    /// \overload
    template <typename TermInst = TerminatorInst>
    auto successors() const {
        return succImpl(terminator<TermInst>());
    }

    template <typename TermInst = TerminatorInst>
    BasicBlock* successor(size_t index) {
        return successors<TermInst>()[utl::narrow_cast<ssize_t>(index)];
    }

    template <typename TermInst = TerminatorInst>
    BasicBlock const* successor(size_t index) const {
        return successors<TermInst>()[utl::narrow_cast<ssize_t>(index)];
    }

    BasicBlock* predecessor(size_t index) { return predecessors()[index]; }

    BasicBlock const* predecessor(size_t index) const {
        return predecessors()[index];
    }

    template <typename TermInst = TerminatorInst>
    size_t numSuccessors() const {
        return successors<TermInst>().size();
    }

    size_t numPredecessors() const { return predecessors().size(); }

    /// \returns `true` if this basic block has exactly one predecessor.
    bool hasSinglePredecessor() const { return numPredecessors() == 1; }

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

    /// \returns `true` if this basic block has exactly one successor.
    template <typename TermInst = TerminatorInst>
    bool hasSingleSuccessor() const {
        return numSuccessors<TermInst>() == 1;
    }

    /// \returns successor if this basic block has a single successor, else
    /// `nullptr`.
    template <typename TermInst = TerminatorInst>
    BasicBlock* singleSuccessor() {
        return const_cast<BasicBlock*>(
            static_cast<BasicBlock const*>(this)->singleSuccessor<TermInst>());
    }

    /// \overload
    template <typename TermInst = TerminatorInst>
    BasicBlock const* singleSuccessor() const {
        return hasSingleSuccessor<TermInst>() ? successors<TermInst>().front() :
                                                nullptr;
    }

private:
    /// For access to insert and erase callbacks.
    friend class Function;

    /// Callbacks used by CFGList base class
    void insertCallback(Instruction&);
    void eraseCallback(Instruction const&);

private:
    utl::small_vector<BasicBlock*> preds;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_BASICBLOCK_H_
