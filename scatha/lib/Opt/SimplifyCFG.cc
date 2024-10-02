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
#include "IR/PassRegistry.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_FUNCTION_PASS(opt::simplifyCFG, "simplifycfg",
                          PassCategory::Simplification, {});

///
static bool removeUnreachableBlocks(Function* function);

///
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

    /// Runs the algorithm
    bool run();

    ///
    void rewriteConstantBranch(BasicBlock* BB);

    /// Try to fold \p BB if it is (nearly) empty and merge the successor and
    /// predecessor blocks if possible
    bool foldIfEmpty(BasicBlock* BB);

    /// Merge single predecessor - single successor edges
    bool foldIntoSinglePred(BasicBlock* BB);

    /// Replaces a branch where the targets are the same with a goto
    bool replaceSameTargetBranch(BasicBlock* BB);

    /// \Returns `true` if we tolerate the speculative execution of \p BB on
    /// execution paths that would otherwise not enter \p BB
    bool canExecuteSpeculatively(BasicBlock const* BB);
};

} // namespace

bool opt::simplifyCFG(ir::Context& ctx, ir::Function& function) {
    bool modified = false;
    while (true) {
        modified |= SCFGContext(ctx, function).run();
        if (removeUnreachableBlocks(&function)) {
            modified = true;
            continue;
        }
        break;
    };
    if (modified) {
        function.invalidateCFGInfo();
    }
    assertInvariants(ctx, function);
    return modified;
}

bool SCFGContext::run() {
    bool modified = false;
    while (!worklist.empty()) {
        auto* BB = *worklist.begin();
        worklist.erase(worklist.begin());
        rewriteConstantBranch(BB);
        if (foldIfEmpty(BB)) {
            modified = true;
            continue;
        }
        if (foldIntoSinglePred(BB)) {
            modified = true;
            continue;
        }
        if (replaceSameTargetBranch(BB)) {
            modified = true;
            continue;
        }
    }
    return modified;
}

static std::optional<bool> getConstantCondition(Branch const* branch) {
    auto* type = cast<IntegralType const*>(branch->condition()->type());
    SC_ASSERT(type->bitwidth() == 1, "Must be boolean");
    // clang-format off
    return SC_MATCH (*branch->condition()) {
        [](IntegralConstant const& cond) {
            return std::optional(cond.value().to<bool>());
        },
        [](UndefValue const&) {
            return std::optional(true);
        },
        [](Value const&) {
            return std::nullopt;
        }
    }; // clang-format on
}

void SCFGContext::rewriteConstantBranch(BasicBlock* BB) {
    auto* branch = dyncast<Branch*>(BB->terminator());
    if (!branch) {
        return;
    }
    auto condition = getConstantCondition(branch);
    if (!condition) {
        return;
    }
    auto* target = *condition ? branch->thenTarget() : branch->elseTarget();
    auto* stale = *condition ? branch->elseTarget() : branch->thenTarget();
    stale->removePredecessor(BB);
    BB->erase(branch);
    BasicBlockBuilder(ctx, BB).add<Goto>(target);
    worklist.insert({ target, stale });
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
        worklist.erase(BB);
        function.erase(BB);
        worklist.insert({ pred, succ });
        worklist.insert(pred->successors().begin(), pred->successors().end());
        worklist.insert(succ->predecessors().begin(),
                        succ->predecessors().end());
        return true;
    }
    /// This is the slightly harder case
    /// We try to replace the phi instructions in `succ` with select
    /// instructions and speculatively move the instructions from `BB` into
    /// `succ`
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
        auto phi = cast<Phi>(succ->extract(itr++));
        auto* select = builder.insert<Select>(insertPoint, condition,
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
    worklist.erase(BB);
    function.erase(BB);
    worklist.erase(pred);
    function.erase(pred);
    /// And insert `succ` and its new predecessors into the worklist
    worklist.insert(succ);
    worklist.insert(succ->predecessors().begin(), succ->predecessors().end());
    return true;
}

bool SCFGContext::foldIntoSinglePred(BasicBlock* BB) {
    auto* pred = BB->singlePredecessor();
    if (!pred || pred->terminator()->numTargets() > 1) {
        return false;
    }
    /// `pred` has only a single successor and is the only predecessor of `BB`,
    /// so we can safely fold `BB` into `pred`
    /// ```
    /// pred
    ///  |
    ///  BB
    /// ```
    eraseSingleValuePhiNodes(BB);
    pred->erase(pred->terminator());
    pred->splice(pred->end(), BB->begin(), BB->end());
    for (auto* succ: pred->successors()) {
        succ->updatePredecessor(BB, pred);
        worklist.insert(succ);
    }
    worklist.insert(pred);
    worklist.erase(BB);
    function.erase(BB);
    return true;
}

bool SCFGContext::replaceSameTargetBranch(BasicBlock* BB) {
    auto* branch = dyncast<Branch*>(BB->terminator());
    if (!branch) {
        return false;
    }
    if (branch->thenTarget() != branch->elseTarget()) {
        return false;
    }
    auto* target = branch->thenTarget();
    target->removePredecessor(BB);
    auto* gotoInst = new Goto(ctx, target);
    BB->insert(branch, gotoInst);
    BB->erase(branch);
    worklist.insert({ BB, target });
    return true;
}

bool SCFGContext::canExecuteSpeculatively(BasicBlock const* BB) {
    /// The number of instructions in a basic block that we are willing to
    /// speculatively execute by folding it into another block
    size_t const MaxInstructionSpec = 2;
    size_t count = 0;
    for (auto& inst: *BB) {
        if (isa<Phi>(inst)) {
            continue;
        }
        /// gep instructions really are no-ops so we ignore them here
        if (isa<GetElementPointer>(inst)) {
            continue;
        }
        if (isa<TerminatorInst>(inst)) {
            break;
        }
        if (hasSideEffects(&inst)) {
            return false;
        }
        ++count;
        if (count > MaxInstructionSpec) {
            return false;
        }
    }
    return true;
}

static bool removeUnreachableBlocks(Function* function) {
    utl::hashset<BasicBlock*> liveBlocks;
    auto dfs = [&](auto& dfs, BasicBlock* BB) -> void {
        if (!liveBlocks.insert(BB).second) {
            return;
        }
        for (auto* succ: BB->successors()) {
            dfs(dfs, succ);
        }
    };
    dfs(dfs, &function->entry());
    bool erasedAny = false;
    for (auto itr = function->begin(); itr != function->end();) {
        auto* BB = itr.to_address();
        if (liveBlocks.contains(BB)) {
            ++itr;
            continue;
        }
        erasedAny = true;
        for (auto* succ: BB->successors()) {
            if (liveBlocks.contains(succ)) {
                succ->removePredecessor(BB);
            }
        }
        itr = function->erase(itr);
    }
    return erasedAny;
}
