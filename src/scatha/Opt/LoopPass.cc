#include "Opt/Passes.h"

#include <iostream>

#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "IR/CFG/Function.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace opt;
using namespace ir;
using namespace ranges::views;

SC_REGISTER_FUNCTION_PASS(
    [](Context& ctx, Function& F, LoopPass const& loopPass,
       PassArgumentMap const&) { return loopSchedule(ctx, F, loopPass); },
    "loop", PassCategory::Schedule, {});

static bool loopCanonicalize(Context& ctx, LNFNode& loop) {
    bool modified = false;
    modified |= simplifyLoop(ctx, loop);
    modified |= makeLCSSA(loop.loopInfo());
    return modified;
}

static utl::small_vector<LNFNode*> computeBFSOrder(LoopNestingForest& LNF) {
    utl::small_vector<LNFNode*> BFSOrder;
    BFSOrder.reserve(LNF.numNodes());
    BFSOrder.insert(BFSOrder.end(), LNF.roots().begin(), LNF.roots().end());
    for (size_t i = 0; i < BFSOrder.size(); ++i) {
        auto* node = BFSOrder[i];
        auto children = node->children();
        BFSOrder.insert(BFSOrder.end(), children.begin(), children.end());
    }
    return BFSOrder;
}

static void validateLNF(Function const& F) {
    // TODO: Only perform this check if F has an existing LNF
    auto& existing = F.getOrComputeLNF();
    auto& mutF = const_cast<Function&>(F);
    auto domSets = DominanceInfo::computeDominatorSets(mutF);
    auto domTree = DominanceInfo::computeDomTree(mutF, domSets);
    auto ref = LoopNestingForest::compute(mutF, domTree);
    auto diffCallback = [&](LNFNode const& A, LNFNode const& B) {
        std::cerr << "Invalid LNF in function " << format(F) << std::endl;
        std::cerr << "Existing LNF: \n";
        print(existing, std::cerr);
        std::cerr << "Reference LNF: \n";
        print(*ref, std::cerr);
        if (A.basicBlock() && B.basicBlock()) {
            std::cerr << "LNFs differ in basic block: ("
                      << format(*A.basicBlock()) << ", "
                      << format(*B.basicBlock()) << ")\n";
        }
    };
    SC_ASSERT(compareEqual(existing, *ref, diffCallback), "LNF is incorrect");
}

bool opt::loopSchedule(Context& ctx, Function& F, LoopPass const& loopPass) {
    auto* LNF = &F.getOrComputeLNF();
    auto BFSOrder = computeBFSOrder(*LNF);
    bool modified = false;
    for (auto* node: BFSOrder | reverse) {
        if (!node->isProperLoop()) {
            continue;
        }
        modified |= loopCanonicalize(ctx, *node);
    }
#if SC_DEBUG
    /// Because we modify the CFG in the canonicalization and patch the
    /// existing LNF, we assert here that the patches are applied correctly
    validateLNF(F);
#endif // SC_DEBUG_LEVEL
    for (auto* node: BFSOrder | reverse) {
        if (!node->isProperLoop()) {
            continue;
        }
        modified |= loopPass(ctx, *node);
    }
    assertInvariants(ctx, F);
    return modified;
}
