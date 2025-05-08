#include "IR/InvariantSetup.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Context.h"

using namespace scatha;
using namespace ir;

/// Makes block \p A a predecessor of block \p B
static void makePred(BasicBlock* A, BasicBlock* B) {
    if (!B->isPredecessor(A)) {
        B->addPredecessor(A);
    }
};

static Metadata const* findReturnMetadata(BasicBlock const& BB) {
    if (!BB.empty()) return BB.back().metadata();
    for (auto* pred: BB.predecessors())
        if (pred) return findReturnMetadata(*pred);
    return nullptr;
}

void ir::setupInvariants(Context& ctx, Function& F) {
    for (auto& BB: F) {
        // Erase everything after the first terminator.
        if (auto itr = ranges::find_if(BB, isa<TerminatorInst>);
            itr != BB.end())
        {
            BB.erase(std::next(itr), BB.end());
        }
        // If we don't have a terminator insert a return.
        if (BB.empty() || !isa<TerminatorInst>(BB.back())) {
            auto* ret = new Return(ctx, ctx.undef(BB.parent()->returnType()));
            ret->setMetadata(clone(findReturnMetadata(BB)));
            BB.pushBack(ret);
        }
        // Setup the predecessor relationship
        auto* terminator = BB.terminator();
        // clang-format off
        SC_MATCH (*terminator) {
            [&](Goto& gt) {
                makePred(&BB, gt.target());
            },
            [&](Branch& br) {
                makePred(&BB, br.thenTarget());
                makePred(&BB, br.elseTarget());
            },
            [&](Return&) {}
        }; // clang-format on
    }
}
