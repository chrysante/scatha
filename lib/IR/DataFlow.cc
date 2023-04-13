#include "IR/DataFlow.h"

#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Loop.h"

using namespace scatha;
using namespace ir;

template <typename T>
static void merge(utl::hashset<T>& dest, auto const& source) {
    for (auto* value: source) {
        dest.insert(value);
    }
}

namespace {

struct LivenessContext {
    //    enum Flags { Visited = 1 << 0, Processed = 1 << 1 };

    using ResultMap = utl::hashmap<BasicBlock const*, BasicBlockLiveSets>;

    LivenessContext(Function const& F, ResultMap& live): F(F), liveSets(live) {}

    void run();

    void dag(BasicBlock const* BB);

    void loopTree(LoopNestingForest::Node const* node);

    utl::hashset<Value const*> phiUses(BasicBlock const* BB);
    utl::hashset<Value const*> phiUses(ranges::range auto&& params);

    Function const& F;
    ResultMap& liveSets;
    /// Need two sets because we use `visited` like a stack to detect back
    /// edges.
    utl::hashset<BasicBlock const*> processed;
    utl::hashset<BasicBlock const*> visited;
    utl::hashset<std::pair<BasicBlock const*, BasicBlock const*>> backEdges;
};

} // namespace

LiveSets LiveSets::compute(Function const& F) {
    LiveSets result;
    LivenessContext ctx(F, result.sets);
    ctx.run();
    return result;
}

void LivenessContext::run() {
    dag(&F.entry());
    auto& LNF = F.getOrComputeLNF();
    for (auto* root: LNF.roots()) {
        loopTree(root);
    }
}

void LivenessContext::dag(BasicBlock const* BB) {
    visited.insert(BB);
    // TODO: Implement modification for irreducible CFGs
    for (auto* succ: BB->successors()) {
        if (visited.contains(succ)) {
            backEdges.insert({ BB, succ });
            continue;
        }
        if (processed.contains(succ)) {
            continue;
        }
        dag(succ);
    }
    auto live = phiUses(BB);
    if (BB->isEntry()) {
        merge(live, phiUses(F.parameters()));
    }
    for (auto* succ: BB->successors()) {
        if (backEdges.contains({ BB, succ })) {
            continue;
        }
        auto liveInSucc = liveSets[succ].liveIn;
        for (auto& phi: succ->phiNodes()) {
            liveInSucc.erase(&phi);
        }
        merge(live, liveInSucc);
    }
    liveSets[BB].liveOut = live;
    for (auto& inst: *BB | ranges::views::reverse) {
        if (isa<Phi>(inst)) {
            break;
        }
        live.erase(&inst);
        for (auto* op: inst.operands()) {
            if (isa<Instruction>(op) || isa<Parameter>(op)) {
                live.insert(op);
            }
        }
    }
    for (auto& phi: BB->phiNodes()) {
        live.insert(&phi);
    }
    liveSets[BB].liveIn = std::move(live);
    processed.insert(BB);
    visited.erase(BB);
}

void LivenessContext::loopTree(LoopNestingForest::Node const* node) {
    /// If this 'loop header' has no children it is a trivial loop aka. not a
    /// loop. Then we don't need to preserve our live-in values.
    if (node->children().empty()) {
        return;
    }
    auto* header  = node->basicBlock();
    auto liveLoop = liveSets[header].liveIn;
    for (auto& phi: header->phiNodes()) {
        liveLoop.erase(&phi);
    }
    /// We also need to merge our own live-out values with the other loop-live
    /// values.
    auto& headerLiveSets = liveSets[header];
    merge(headerLiveSets.liveIn, liveLoop);
    merge(headerLiveSets.liveOut, liveLoop);
    for (auto* child: node->children()) {
        auto& childLiveSets = liveSets[child->basicBlock()];
        merge(childLiveSets.liveIn, liveLoop);
        merge(childLiveSets.liveOut, liveLoop);
        loopTree(child);
    }
}

static constexpr auto take_address =
    ranges::views::transform([](auto& x) { return &x; });

static constexpr auto phiUseFilter = ranges::views::filter(
                                         [](Value const& value) {
    return ranges::any_of(value.users(),
                          [](auto* user) { return isa<Phi>(user); });
}) | take_address;

utl::hashset<Value const*> LivenessContext::phiUses(BasicBlock const* BB) {
    return *BB | phiUseFilter | ranges::to<utl::hashset<Value const*>>;
}

utl::hashset<Value const*> LivenessContext::phiUses(
    ranges::range auto&& params) {
    return params | phiUseFilter | ranges::to<utl::hashset<Value const*>>;
}
