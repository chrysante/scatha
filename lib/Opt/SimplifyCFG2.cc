#include "Opt/Passes.h"

#include <queue>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::simplifyCFG2, "simplifycfg2");

static void eraseSingleValuePhiNodes(BasicBlock* BB) {
    SC_ASSERT(BB->hasSinglePredecessor(), "Precondition");
    Phi* phi = nullptr;
    while ((phi = dyncast<Phi*>(&BB->front()))) {
        SC_ASSERT(phi->numOperands() == 1,
                  "Must be one operand in a single predecessor block");
        phi->replaceAllUsesWith(phi->operandAt(0));
        BB->erase(phi);
    }
}

namespace {

struct SCFGContext {
    Context& ctx;
    Function& function;

    utl::hashset<BasicBlock*> worklist;

    SCFGContext(Context& ctx, Function& function):
        ctx(ctx),
        function(function),
        worklist(function | TakeAddress |
                 ranges::to<utl::hashset<BasicBlock*>>) {}

    bool run();

    bool foldIfEmpty(BasicBlock* BB);

    bool canExecuteSpeculatively(BasicBlock const* BB);
};

} // namespace

bool opt::simplifyCFG2(ir::Context& ctx, ir::Function& function) {
    return SCFGContext(ctx, function).run();
}

bool SCFGContext::run() {
    bool modified = false;
    while (!worklist.empty()) {
        auto* BB = *worklist.begin();
        worklist.erase(worklist.begin());
        if (foldIfEmpty(BB)) {
            modified = true;
            continue;
        }
    }
    if (modified) {
        function.invalidateCFGInfo();
    }
    return modified;
}

bool SCFGContext::foldIfEmpty(BasicBlock* BB) {
    auto* pred = BB->singlePredecessor();
    auto* succ = BB->singleSuccessor();
    if (!pred || !succ) {
        return false;
    }
    if (!succ->isPredecessor(pred) || !succ->hasPhiNodes()) {
        /// This is the simple case, we can just update the edges if `BB` is
        /// empty
        if (!BB->emptyExceptTerminator()) {
            return false;
        }
        /// ```
        /// pred
        ///  |
        ///  BB
        ///  |
        /// succ
        /// ```
        succ->updatePredecessor(BB, pred);
        pred->terminator()->updateOperand(BB, succ);
        function.erase(BB);
        worklist.insert({ pred, succ });
        worklist.insert(pred->successors().begin(), pred->successors().end());
        worklist.insert(succ->predecessors().begin(),
                        succ->predecessors().end());
        return true;
    }
    /// This is the slightly harder case
    /// We try replace the phi instructions in `succ` with select instructions
    /// and speculatively move the instructions from `BB` into `succ`
    /// ```
    ///   pred
    ///  /   |
    /// BB   |
    ///  \   |
    ///   succ
    /// ```

    /// We can only create select instructions with two operands
    if (succ->numPredecessors() != 2 || pred->numSuccessors() != 2) {
        return false;
    }
    ///
    if (!canExecuteSpeculatively(BB)) {
        return false;
    }
    /// We replace all phi instructions in `succ` with select instructions
    auto* branch = cast<Branch*>(pred->terminator());
    auto* condition = branch->condition();
    auto* insertPoint = succ->phiEnd().to_address();
    auto* thenTarget = branch->thenTarget();
    auto* elseTarget = branch->elseTarget();
    if (thenTarget == succ) {
        thenTarget = pred;
    }
    if (elseTarget == succ) {
        elseTarget = pred;
    }
    BasicBlockBuilder builder(ctx, succ);
    for (auto itr = succ->begin(); isa<Phi>(*itr);) {
        auto phi = uniquePtrCast<Phi>(succ->extract(itr++));
        auto* select = builder.insert<Select>(insertPoint,
                                              condition,
                                              phi->operandOf(thenTarget),
                                              phi->operandOf(elseTarget),
                                              std::string(phi->name()));
        phi->replaceAllUsesWith(select);
    }
    /// We splice `BB` into `succ`
    eraseSingleValuePhiNodes(BB);
    BB->erase(BB->terminator());
    succ->splice(succ->begin(), BB->begin(), BB->end());
    /// Then we splice `pred` into `succ`
    pred->erase(pred->terminator());
    succ->splice(succ->begin(), pred->begin(), pred->end());
    /// Then we update the edges in the CFG
    for (auto* p: pred->predecessors()) {
        p->terminator()->updateOperand(pred, succ);
    }
    succ->setPredecessors(pred->predecessors());
    /// And erase the dead blocks
    function.erase(BB);
    function.erase(pred);
    worklist.erase(pred);
    /// And insert `succ` and its new predecessors into the worklist
    worklist.insert(succ);
    ranges::for_each(succ->predecessors(),
                     [&](auto* pred) { worklist.insert(pred); });
    return true;
}

bool SCFGContext::canExecuteSpeculatively(BasicBlock const* BB) {
    size_t count = 0;
    for (auto& inst: *BB) {
        if (isa<TerminatorInst>(inst)) {
            break;
        }
        if (hasSideEffects(&inst)) {
            return false;
        }
        ++count;
        if (count > 4) {
            return false;
        }
    }
    return true;
}
