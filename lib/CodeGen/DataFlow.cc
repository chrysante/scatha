#include "CodeGen/Passes.h"

#include <range/v3/view.hpp>

#include "IR/CFG/Function.h"
#include "IR/Loop.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

template <typename T>
static void merge(utl::hashset<T>& dest, auto const& source) {
    for (auto* value: source) {
        dest.insert(value);
    }
}

namespace {

struct LivenessContext {
    LivenessContext(mir::Function& F): F(F) {}

    void run();

    void dag(mir::BasicBlock* BB);

    void loopTree(ir::LoopNestingForest::Node const* node);

    utl::hashset<mir::Register*> phiUses(mir::BasicBlock* BB);
    utl::hashset<mir::Register*> phiUses(ranges::range auto&& params);

    mir::Function& F;
    /// Need two sets because we use `visited` like a stack to detect back
    /// edges.
    utl::hashset<mir::BasicBlock const*> processed;
    utl::hashset<mir::BasicBlock const*> visited;
    utl::hashmap<ir::BasicBlock const*, mir::BasicBlock*> bbMap;
    utl::hashset<std::pair<mir::BasicBlock const*, mir::BasicBlock const*>>
        backEdges;
};

} // namespace

void cg::computeLiveSets(mir::Function& F) {
    LivenessContext ctx(F);
    ctx.run();
}

void LivenessContext::run() {
    for (auto& BB: F) {
        bbMap[BB.irBasicBlock()] = &BB;
    }
    dag(F.entry());
    auto& LNF = F.irFunction()->getOrComputeLNF();
    for (auto* root: LNF.roots()) {
        loopTree(root);
    }
}

void LivenessContext::dag(mir::BasicBlock* BB) {
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
        merge(live, phiUses(F.ssaArgumentRegisters()));
    }
    for (auto* succ: BB->successors()) {
        if (backEdges.contains({ BB, succ })) {
            continue;
        }
        auto liveInSucc = succ->liveIn();
        for (auto& phi: *succ) {
            if (phi.instcode() != mir::InstCode::Phi) {
                break;
            }
            liveInSucc.erase(phi.dest());
        }
        merge(live, liveInSucc);
    }
    BB->setLiveOut(live);
    for (auto& inst: *BB | ranges::views::reverse) {
        if (inst.instcode() == mir::InstCode::Phi) {
            break;
        }
        live.erase(inst.dest());
        for (auto* op: inst.operands()) {
            if (auto* reg = dyncast_or_null<mir::SSARegister*>(op)) {
                live.insert(reg);
            }
        }
    }
    for (auto& phi: *BB) {
        if (phi.instcode() != mir::InstCode::Phi) {
            break;
        }
        live.insert(phi.dest());
    }
    BB->setLiveIn(std::move(live));
    processed.insert(BB);
    visited.erase(BB);
}

void LivenessContext::loopTree(ir::LoopNestingForest::Node const* node) {
    /// If this 'loop header' has no children it is a trivial loop aka. not a
    /// loop. Then we don't need to preserve our live-in values.
    if (node->children().empty()) {
        return;
    }
    mir::BasicBlock* header = bbMap[node->basicBlock()];
    auto liveLoop = header->liveIn();
    for (auto& phi: *header) {
        if (phi.instcode() != mir::InstCode::Phi) {
            break;
        }
        liveLoop.erase(phi.dest());
    }
    /// We also need to merge our own live-out values with the other loop-live
    /// values.
    for (auto* r: liveLoop) {
        header->addLiveIn(r);
        header->addLiveOut(r);
    }
    for (auto* child: node->children()) {
        mir::BasicBlock* loopBlock = bbMap[child->basicBlock()];
        for (auto* r: liveLoop) {
            loopBlock->addLiveIn(r);
            loopBlock->addLiveOut(r);
        }
        loopTree(child);
    }
}

static constexpr auto phiUseFilter =
    ranges::views::filter([](mir::Register const* reg) -> bool {
        if (!reg) {
            return false;
        }
        return ranges::any_of(reg->uses(), [](auto* user) {
            return user->instcode() == mir::InstCode::Phi;
        });
    });

utl::hashset<mir::Register*> LivenessContext::phiUses(mir::BasicBlock* BB) {
    return *BB | ranges::views::transform([](mir::Instruction& inst) {
        return inst.dest();
    }) | phiUseFilter |
           ranges::to<utl::hashset<mir::Register*>>;
}

utl::hashset<mir::Register*> LivenessContext::phiUses(
    ranges::range auto&& params) {
    return params | phiUseFilter | ranges::to<utl::hashset<mir::Register*>>;
}
