#include "CodeGen/Passes.h"

#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "MIR/CFG.h"
#include "MIR/Clone.h"
#include "MIR/Instructions.h"

/// Jump elision tries to reorder the basic blocks in such a way that as many
/// edges as possible appear adjacent in the internal list of the function.
/// Then we can erase terminating jumps because control flow just _flows
/// through_ to the next basic block.
///
/// This is archieved with a depth first search over the function.

using namespace scatha;
using namespace cg;
using namespace mir;

namespace {

struct JumpElimContext {
    /// The move looks sketchy, but is fine. We move all basic blocks of `F`
    /// into `L`, and during DFS we gradually move them back into `F`
    JumpElimContext(Function& F): F(F), L(std::move(F)) {}

    ~JumpElimContext();

    void run();

    void DFS(BasicBlock* BB);

    void removeJumps();

    Function& F;
    CFGList<Function, BasicBlock> L;
    utl::hashset<BasicBlock const*> visited;
};

} // namespace

void cg::elideJumps(mir::Context&, Function& F) { JumpElimContext(F).run(); }

void JumpElimContext::run() {
    DFS(&L.front());
    removeJumps();
}

void JumpElimContext::DFS(BasicBlock* BB) {
    if (visited.contains(BB)) {
        return;
    }
    visited.insert(BB);
    L.extract(BB).release();
    F.pushBack(BB);
    SC_ASSERT(!BB->empty(), "");

    for (auto* term = dyncast<JumpBase*>(&BB->back()); term != nullptr;
         term = dyncast<JumpBase*>(term->prev()))
    {
        /// Target could also be a function.
        if (auto* targetBB = dyncast<BasicBlock*>(term->target())) {
            DFS(targetBB);
        }
        if (term == &BB->front()) {
            break;
        }
    }
    auto* term = dyncast<JumpBase*>(&BB->back());
    if (!term) {
        return;
    }
    /// If the destination of the jump is the next block, we just fall through
    if (term->target() == BB->next()) {
        return;
    }
    /// If the destination block only contains a terminator or a conditional
    /// jump, we copy these instructions into our block
    auto* next = dyncast<BasicBlock*>(term->target());
    /// `next` could also be a function
    if (!next) {
        return;
    }
    auto const nextNumInst = ranges::distance(*next);
    switch (nextNumInst) {
    case 1: {
        auto* nextTerm = cast<TerminatorInst*>(&next->back());
        auto* newTerm = clone(*nextTerm).release();
        BB->erase(term);
        BB->pushBack(newTerm);
        next->removePredecessor(BB);
        BB->removeSuccessor(next);
        auto* newJump = dyncast<JumpBase*>(newTerm);
        if (newJump && isa<BasicBlock>(newJump->target())) {
            auto* dest = cast<BasicBlock*>(newJump->target());
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

/// \Warning this is linear in the number of instructions in \p BB
static bool hasJumpsTo(BasicBlock const* BB, Value const* dest) {
    return ranges::any_of(*BB, [&](Instruction const& inst) {
        auto* jump = dyncast<JumpBase const*>(&inst);
        return jump && jump->target() == dest;
    });
}

void JumpElimContext::removeJumps() {
    for (auto& BB: F) {
        /// After we will splice `next` into `BB`, there might be another jump
        /// at the end that we can now elide, so we repeat the process for the
        /// current block
        while (!BB.empty()) {
            auto* jump = dyncast<JumpInst*>(&BB.back());
            if (!jump) {
                break;
            }
            auto* next = [&]() -> Value* {
                if (&BB != &F.back()) {
                    return BB.next();
                }
                return F.next();
            }();
            if (jump->target() != next) {
                break;
            }
            /// We erase the jump if the target is the next block
            BB.erase(jump);
            /// If the next block is a BB that has is not the target of other
            /// jump, we splice the blocks
            auto* nextBB = dyncast<BasicBlock*>(next);
            if (!nextBB) {
                break;
            }
            if (nextBB->predecessors().size() > 1) {
                break;
            }
            /// Even if `nextBB` has no other predecessors than `BB`, at this
            /// point there could be other conditional jumps in `BB` to
            /// `nextBB`. Then we can't merge the blocks
            if (hasJumpsTo(&BB, nextBB)) {
                break;
            }
            SC_ASSERT(&BB == nextBB->predecessors().front(), "");
            BB.splice(BB.end(), nextBB);
            auto nextSuccessors = nextBB->successors() | ToSmallVector<>;
            for (auto* succ: nextSuccessors) {
                succ->removePredecessor(nextBB);
                succ->addPredecessor(&BB);
            }
            F.erase(nextBB);
        }
    }
}

JumpElimContext::~JumpElimContext() {
    /// We update the predecessor lists of the successors of the erased blocks
    for (auto& BB: L) {
        for (auto* succ: BB.successors()) {
            succ->removePredecessor(&BB);
        }
    }
}
