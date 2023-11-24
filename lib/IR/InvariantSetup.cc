#include "IR/InvariantSetup.h"

#include "IR/CFG.h"
#include "IR/Context.h"

using namespace scatha;
using namespace ir;

/// Makes block \p A a predecessor of block \p B
static void makePred(BasicBlock* A, BasicBlock* B) {
    if (!B->isPredecessor(A)) {
        B->addPredecessor(A);
    }
};

void ir::setupInvariants(Context& ctx, Function& function) {
    for (auto& bb: function) {
        /// Erase everything after the first terminator.
        for (auto itr = bb.begin(); itr != bb.end(); ++itr) {
            if (isa<TerminatorInst>(*itr)) {
                bb.erase(std::next(itr), bb.end());
                break;
            }
        }
        /// If we don't have a terminator insert a return.
        if (bb.empty() || !isa<TerminatorInst>(bb.back())) {
            bb.pushBack(new Return(ctx, ctx.undef(bb.parent()->returnType())));
            continue;
        }
        auto* terminator = bb.terminator();
        // clang-format off
        SC_MATCH (*terminator) {
            [&](Goto& gt) {
                makePred(&bb, gt.target());
            },
            [&](Branch& br) {
                makePred(&bb, br.thenTarget());
                makePred(&bb, br.elseTarget());
            },
            [&](Return&) {}
        }; // clang-format on
    }
}
