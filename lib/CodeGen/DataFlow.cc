#include "CodeGen/Passes.h"

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "CodeGen/TargetInfo.h"
#include "IR/CFG.h"
#include "IR/Loop.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace cg;
using namespace mir;

/// # Live in and live out set computation on SSA form

template <typename T>
static void merge(utl::hashset<T>& dest, auto const& source) {
    for (auto* value: source) {
        dest.insert(value);
    }
}

namespace {

struct LivenessContext {
    LivenessContext(Function& F): F(F) {}

    void run();

    void dag(BasicBlock* BB);

    void loopTree(ir::LNFNode const* node);

    utl::hashset<Register*> phiUses(BasicBlock* BB);
    utl::hashset<Register*> phiUses(ranges::range auto&& params);

    Function& F;
    /// Need two sets because we use `visited` like a stack to detect back
    /// edges.
    utl::hashset<BasicBlock const*> processed;
    utl::hashset<BasicBlock const*> visited;
    utl::hashmap<ir::BasicBlock const*, BasicBlock*> bbMap;
    utl::hashset<std::pair<BasicBlock const*, BasicBlock const*>> backEdges;
};

} // namespace

void cg::computeLiveSets(Context& ctx, Function& F) {
    LivenessContext(F).run();
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

void LivenessContext::dag(BasicBlock* BB) {
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
    /// _Live out_ are all registers defined in this block that are used by phi
    /// instructions
    utl::hashset<Register*> live = phiUses(BB);
    if (BB->isEntry()) {
        merge(live, phiUses(F.ssaArgumentRegisters()));
    }
    /// Also any registers that are _live in_ in our successors are _live out_
    /// in this block unless they are defined by phi instructions in the
    /// successor.
    /// Also all registers that are used by phi nodes in our successor
    /// corresponding to this block
    for (auto succ: BB->successors()) {
        auto liveInSucc = succ->liveIn();
        size_t phiIndex =
            utl::narrow_cast<size_t>(ranges::find(succ->predecessors(), BB) -
                                     succ->predecessors().begin());
        for (auto& phi: succ->phiNodes()) {
            /// TODO: Reevaluate this
            /// This fixes liveness issues when phi instructions use values
            /// defined earlier than in the predecessor block
            auto* ourOperand = phi.operandAt(phiIndex);
            if (auto* reg = dyncast<SSARegister*>(ourOperand)) {
                liveInSucc.insert(reg);
            }
        }
        if (!backEdges.contains({ BB, succ })) {
            for (auto& phi: succ->phiNodes()) {
                liveInSucc.erase(phi.dest());
            }
        }
        merge(live, liveInSucc);
    }
    /// If we return from this block, the return values are live out
    if (auto* ret = dyncast<ReturnInst*>(&BB->back())) {
        for (auto* reg: ret->operands() | Filter<Register>) {
            live.insert(reg);
        }
    }
    BB->setLiveOut(live);
    /// After we set the _live out_ set we remove instructions from the set to
    /// determine our _live in_ set. We erase all registers that are defined by
    /// our instructions. We add all registers that are used by our
    /// instructions. If these are also defined here they will be removed again
    /// because we traverse the instructions in reverse order.
    for (auto& inst: *BB | ranges::views::reverse) {
        if (isa<PhiInst>(inst)) {
            break;
        }
        live.erase(inst.dest());
        for (auto* op: inst.operands()) {
            if (auto* reg = dyncast_or_null<SSARegister*>(op)) {
                live.insert(reg);
            }
        }
    }
    /// Also all registers defined by phi instructions are _live in_
    for (auto& phi: BB->phiNodes()) {
        live.insert(phi.dest());
    }
    BB->setLiveIn(std::move(live));
    processed.insert(BB);
    visited.erase(BB);
}

static bool isTrivialLoop(ir::LNFNode const* node) {
    if (!node->children().empty()) {
        return false;
    }
    auto* BB = node->basicBlock();
    return !ranges::contains(BB->successors(), BB);
}

void LivenessContext::loopTree(ir::LNFNode const* node) {
    /// If this 'loop header' is a trivial loop aka. not a
    /// loop, we don't need to preserve our live-in values.
    if (isTrivialLoop(node)) {
        return;
    }
    BasicBlock* header = bbMap[node->basicBlock()];
    auto liveLoop = header->liveIn();
    for (auto& phi: header->phiNodes()) {
        liveLoop.erase(phi.dest());
    }
    /// We also need to merge our own live-out values with the other loop-live
    /// values.
    for (auto* r: liveLoop) {
        header->addLiveIn(r);
        header->addLiveOut(r);
    }
    for (auto* child: node->children()) {
        BasicBlock* loopBlock = bbMap[child->basicBlock()];
        for (auto* r: liveLoop) {
            loopBlock->addLiveIn(r);
            loopBlock->addLiveOut(r);
        }
        loopTree(child);
    }
}

static constexpr auto PhiUseFilter =
    ranges::views::filter([](Register const* reg) -> bool {
        if (!reg) {
            return false;
        }
        return ranges::any_of(reg->uses(), isa<PhiInst>);
    });

static constexpr auto Dests =
    ranges::views::transform([](Instruction& inst) { return inst.dest(); });

/// Returns all registers defined by instructions in \p BB that are used by phi
/// instructions
utl::hashset<Register*> LivenessContext::phiUses(BasicBlock* BB) {
    return *BB | Dests | PhiUseFilter | ranges::to<utl::hashset<Register*>>;
}

utl::hashset<Register*> LivenessContext::phiUses(ranges::range auto&& params) {
    return params | PhiUseFilter | ranges::to<utl::hashset<Register*>>;
}

/// # Live range computation on virtual register form

#include "MIR/Print.h"
#include <iostream>
#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

struct LiveRange {
    int begin;
    int end;

    bool operator==(LiveRange const&) const = default;

    auto operator<=>(LiveRange const& rhs) const { return begin <=> rhs.begin; }
};

std::ostream& operator<<(std::ostream& str, LiveRange const& range) {
    return str << "[" << range.begin << ", " << range.end << ")";
}

using namespace ranges::views;

static LiveRange computeLiveRange(Function& F,
                                  BasicBlock& BB,
                                  Register* reg,
                                  int32_t begin) {
    int32_t end = begin;
    int32_t first =
        std::max(begin + 1, static_cast<int32_t>(BB.front().index()));
    for (auto progPoint: F.programPoints() | drop(first)) {
        if (std::holds_alternative<BasicBlock*>(progPoint)) {
            break;
        }
        auto* inst = std::get<Instruction*>(progPoint);
        if (inst->parent() != &BB) {
            break;
        }
        /// Live range extends to at least the last use of the register in this
        /// block
        if (ranges::contains(inst->operands(), reg)) {
            end = utl::narrow_cast<int32_t>(inst->index());
        }
        /// Live range ends at the last use before a def
        if (ranges::contains(inst->destRegisters(), reg)) {
            return { begin, end };
        }
        /// Calls clobber all callee registers
        if (isa<CallBase>(inst) && isa<CalleeRegister>(reg)) {
            return { begin, end };
        }
    }
    if (BB.liveOut().contains(reg)) {
        return { begin, BB.back().index() + 1 };
    }
    return { begin, end };
}

void cg::computeLiveRanges(Context& ctx, Function& F) {
    utl::hashmap<Register*, utl::small_vector<LiveRange>> liveRangeMap;
    for (auto& reg: concat(F.virtualRegisters() | transform(cast<Register&>),
                           F.calleeRegisters()))
    {
        auto& liveRanges = liveRangeMap[&reg];
        for (auto& BB: F) {
            if (BB.liveIn().contains(&reg)) {
                auto range = computeLiveRange(F, BB, &reg, BB.index());
                liveRanges.push_back(range);
            }
        }
        for (auto* call: F.linearInstructions() | Filter<CallBase>) {
            size_t numMDRegs =
                isa<CallInst>(call) ? numRegistersForCallMetadata() : 0;
            if (ranges::contains(F.calleeRegisters() | TakeAddress |
                                     take(numMDRegs +
                                          call->numReturnRegisters()),
                                 &reg))
            {
                auto range =
                    computeLiveRange(F, *call->parent(), &reg, call->index());
                liveRanges.push_back(range);
            }
        }
        for (auto* def: reg.defs()) {
            auto range =
                computeLiveRange(F, *def->parent(), &reg, def->index());
            liveRanges.push_back(range);
        }
        ranges::sort(liveRanges.begin(), liveRanges.end());
    }

    /// For now we just print the results
    std::cout << tfmt::format(tfmt::Bold, "Live ranges for ") << F << std::endl;
    for (auto& [reg, liveRanges]: liveRangeMap) {
        std::cout << *reg << ": ";
        for (bool first = true; auto range: liveRanges) {
            if (!first) std::cout << ", ";
            first = false;
            std::cout << range;
        }
        std::cout << std::endl;
    }
}
