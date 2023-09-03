#include "CodeGen/Passes.h"

#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "MIR/CFG.h"

/// Jump elision tries to reorder the basic blocks in such a way that as many
/// edges as possible appear adjacent in the internal list of the function. Then
/// we can erase terminating jumps because control flow just _flows through_ to
/// the next basic block.
///
/// This is archieved with a simple depth first search over the function.

using namespace scatha;
using namespace cg;

namespace {

struct JumpElimContext {
    /// The move looks sketchy, but is fine. We move all basic blocks of `F`
    /// into `L`, and during DFS we gradually move them back into `F`
    JumpElimContext(mir::Function& F): F(F), L(std::move(F)) {}

    void run();

    void DFS(mir::BasicBlock* BB);

    void removeJumps();

    mir::Function& F;
    CFGList<mir::Function, mir::BasicBlock> L;
    utl::hashset<mir::BasicBlock const*> visited;
};

} // namespace

void cg::elideJumps(mir::Function& F) { JumpElimContext(F).run(); }

void JumpElimContext::run() {
    DFS(&L.front());
    removeJumps();
}

void JumpElimContext::DFS(mir::BasicBlock* BB) {
    if (visited.contains(BB)) {
        return;
    }
    visited.insert(BB);
    L.extract(BB).release();
    F.pushBack(BB);
    auto* term = &BB->back();
    while (mir::isJump(term->instcode())) {
        auto* target = term->operandAt(0);
        /// Target could also be a function.
        if (auto* targetBB = dyncast<mir::BasicBlock*>(target)) {
            DFS(targetBB);
        }
        term = term->prev();
    }
    term = &BB->back();
    if (term->instcode() != mir::InstCode::Jump) {
        return;
    }
    /// If the destination of the jump is the next block, we just fall through
    if (term->operandAt(0) == BB->next()) {
        return;
    }
    /// If the destination block only contains a terminator or a conditional
    /// jump, we copy these instructions into our block
    auto* next = dyncast<mir::BasicBlock*>(term->operandAt(0));
    /// `next` could also be a function
    if (!next) {
        return;
    }
    auto const nextNumInst = ranges::distance(*next);
    switch (nextNumInst) {
    case 1: {
        auto* nextTerm = &next->back();
        SC_ASSERT(isTerminator(nextTerm->instcode()), "");
        auto* newTerm = nextTerm->clone().release();
        BB->erase(term);
        BB->pushBack(newTerm);
        next->removePredecessor(BB);
        BB->removeSuccessor(next);
        if (isJump(newTerm->instcode())) {
            auto* dest = cast<mir::BasicBlock*>(newTerm->operandAt(0));
            dest->addPredecessor(BB);
            BB->addSuccessor(dest);
        }
        break;
    }
    case 3: {
        /// TODO: If next looks like this:
        /// ```
        /// test/cmp ...
        /// <cond-jump> ...
        /// jump/ret ...
        /// ```
        /// we can also copy these three instructions into our block and elide
        /// the jump
        break;
    }

    default:
        break;
    }
}

static bool hasJumpsTo(mir::BasicBlock const* BB, mir::BasicBlock const* dest) {
    return ranges::any_of(*BB, [&](mir::Instruction const& inst) {
        if (!isJump(inst.instcode())) {
            return false;
        }
        return inst.operandAt(0) == dest;
    });
}

void JumpElimContext::removeJumps() {
    for (auto& BB: F) {
begin:
        auto* jump = &BB.back();
        if (jump->instcode() != mir::InstCode::Jump) {
            continue;
        }
        auto* next = BB.next();
        if (jump->operandAt(0) != next) {
            continue;
        }
        BB.erase(jump);
        if (next->predecessors().size() > 1) {
            continue;
        }
        /// Even if `next` has no other predecessors than `BB`, at this point
        /// there could be other conditional jumps in `BB` to `next`. Then we
        /// can't merge the blocks
        if (hasJumpsTo(&BB, next)) {
            continue;
        }
        SC_ASSERT(&BB == next->predecessors().front(), "");
        BB.splice(BB.end(), next);
        auto nextSuccessors = next->successors() | ToSmallVector<>;
        for (auto* succ: nextSuccessors) {
            succ->removePredecessor(next);
            succ->addPredecessor(&BB);
        }
        F.erase(next);
        /// After we spliced `next` into `BB`, there might be another jump at
        /// the end that we can now elide, so we repeat the current loop
        /// iteration
        goto begin;
    }
}
