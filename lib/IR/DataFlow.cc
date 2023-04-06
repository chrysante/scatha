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

    using ResultMap =
        utl::hashmap<BasicBlock const*, utl::hashset<Instruction const*>>;

    LivenessContext(ResultMap& in, ResultMap& out): liveIn(in), liveOut(out) {}

    void run(Function const& F);

    void dag(BasicBlock const* BB);

    void loopTree(LoopNestingForest::Node const* node);

    utl::hashset<Instruction const*> phiUses(BasicBlock const* BB);

    ResultMap& liveIn;
    ResultMap& liveOut;
    utl::hashmap<BasicBlock const*, int> flags;
    utl::hashset<std::pair<BasicBlock const*, BasicBlock const*>> backEdges;
};

} // namespace

LiveSets ir::computeLiveSets(Function const& F) {
    LiveSets result;
    LivenessContext ctx(result.in, result.out);
    ctx.run(F);
    return result;
}

void LivenessContext::run(Function const& F) {
    dag(&F.entry());
    auto& LNF = F.getOrComputeLNF();
    for (auto* root: LNF.roots()) {
        loopTree(root);
    }
}

void LivenessContext::dag(BasicBlock const* BB) {
    flags[BB] |= Visited;
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
    for (auto* succ: BB->successors()) {
        if (backEdges.contains({ BB, succ })) {
            continue;
        }
        auto liveInSucc = liveIn[succ];
        for (auto& phi: succ->phiNodes()) {
            liveInSucc.erase(&phi);
        }
        merge(live, liveInSucc);
    }
    liveOut[BB] = live;
    for (auto& inst: *BB | ranges::views::reverse) {
        if (isa<Phi>(inst)) {
            break;
        }
        live.erase(&inst);
        for (auto* operand: inst.operands()) {
            if (auto* instOp = dyncast<Instruction const*>(operand)) {
                live.insert(instOp);
            }
        }
    }
    for (auto& phi: BB->phiNodes()) {
        live.insert(&phi);
    }
    liveIn[BB] = std::move(live);
    flags[BB] |= Processed;
}

void LivenessContext::loopTree(LoopNestingForest::Node const* node) {
    if (node->children().empty()) {
        return;
    }
    auto* header  = node->basicBlock();
    auto liveLoop = liveIn[header];
    for (auto& phi: header->phiNodes()) {
        liveLoop.erase(&phi);
    }
    for (auto* child: node->children()) {
        auto* header = child->basicBlock();
        merge(liveIn[header], liveLoop);
        merge(liveOut[header], liveLoop);
        loopTree(child);
    }
}

static constexpr auto take_address =
    ranges::views::transform([](auto& x) { return &x; });

utl::hashset<Instruction const*> LivenessContext::phiUses(
    BasicBlock const* BB) {
    return *BB | ranges::views::filter([](Instruction const& inst) {
        return ranges::any_of(inst.users(),
                              [](auto* user) { return isa<Phi>(user); });
    }) | take_address |
           ranges::to<utl::hashset<Instruction const*>>;
}
