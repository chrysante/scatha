#include "IR/DataFlow.h"

#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Loop.h"

using namespace scatha;
using namespace ir;

template <typename T>
static void merge(utl::hashset<T>& dest, auto const& source) {
    for (auto* BB: source) {
        dest.insert(BB);
    }
}

namespace {

struct LivenessContext {
    enum Flags { Visited = 1 << 0, Processed = 1 << 1 };

    using ResultMap = utl::hashmap<BasicBlock const*, BasicBlockLiveSets>;

    LivenessContext(Function const& F, ResultMap& live): F(F), liveSets(live) {}

    void run();

    void dag(BasicBlock const* BB);

    void loopTree(LoopNestingForest::Node const* node);

    utl::hashset<Value const*> phiUses(BasicBlock const* BB);
    utl::hashset<Value const*> phiUses(ranges::range auto&& params);

    Function const& F;
    ResultMap& liveSets;
    utl::hashmap<BasicBlock const*, int> flags;
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
    flags[BB] |= Visited;
    // TODO: Implement modification for irreducible CFGs
    for (auto* succ: BB->successors()) {
        auto const succFlag = flags[succ];
        if (succFlag & Visited) {
            backEdges.insert({ BB, succ });
            continue;
        }
        if (succFlag & Processed) {
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
    flags[BB] |= Processed;
}

void LivenessContext::loopTree(LoopNestingForest::Node const* node) {
    if (node->children().empty()) {
        return;
    }
    auto* header  = node->basicBlock();
    auto liveLoop = liveSets[header].liveIn;
    for (auto& phi: header->phiNodes()) {
        liveLoop.erase(&phi);
    }
    for (auto* child: node->children()) {
        auto* header = child->basicBlock();
        merge(liveSets[header].liveIn, liveLoop);
        merge(liveSets[header].liveOut, liveLoop);
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
