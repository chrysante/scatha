#include "Opt/Common.h"

#include <range/v3/view.hpp>
#include <utl/hashset.hpp>

#include "Common/Graph.h"
#include "IR/CFG.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_PASS(opt::splitCriticalEdges, "splitcriticaledges");

bool opt::preceeds(Instruction const* a, Instruction const* b) {
    SC_ASSERT(a->parent() == b->parent(),
              "a and b must be in the same basic block");
    auto* bb = a->parent();
    auto const* const end = bb->end().to_address();
    for (; a != end; a = a->next()) {
        if (a == b) {
            return true;
        }
    }
    return false;
}

bool opt::isReachable(Instruction const* from, Instruction const* to) {
    SC_ASSERT(
        from != to,
        "from and to are equal. Does that mean they are reachable or not?");
    SC_ASSERT(from->parentFunction() == to->parentFunction(),
              "The instructions must be in the same function for this to be "
              "sensible");
    if (from->parent() == to->parent()) {
        /// From and to are in the same basic block. If \p *from preceeds \p *to
        /// then \p *to is definitely reachable.
        if (preceeds(from, to)) {
            return true;
        }
    }
    /// If they are not in the same basic block or \p *to comes before \p *from
    /// perform a DFS to check if we can reach the BB of \p *to from the BB of
    /// \p *from.
    utl::hashset<BasicBlock const*> visited;
    auto search = [&, target = to->parent()](BasicBlock const* bb,
                                             auto& search) -> bool {
        visited.insert(bb);
        if (bb == target) {
            return true;
        }
        for (BasicBlock const* succ: bb->successors()) {
            if (visited.contains(succ)) {
                continue;
            }
            if (search(succ, search)) {
                return true;
            }
        }
        return false;
    };
    return search(from->parent(), search);
}

static bool cmpEqImpl(ir::Phi const* lhs, auto rhs) {
    if (lhs->argumentCount() != ranges::size(rhs)) {
        return false;
    }
    auto lhsArgs = ranges::views::common(lhs->arguments());
    utl::hashset<ConstPhiMapping> lhsSet(lhsArgs.begin(), lhsArgs.end());
    auto rhsCommon = ranges::views::common(rhs);
    utl::hashset<ConstPhiMapping> rhsSet(rhsCommon.begin(), rhsCommon.end());
    return lhsSet == rhsSet;
}

bool opt::compareEqual(ir::Phi const* lhs,
                       std::span<ir::ConstPhiMapping const> rhs) {
    return cmpEqImpl(lhs, rhs);
}

bool opt::compareEqual(ir::Phi const* lhs,
                       std::span<ir::PhiMapping const> rhs) {
    return cmpEqImpl(lhs, rhs);
}

void opt::removePredecessorAndUpdatePhiNodes(
    ir::BasicBlock* basicBlock, ir::BasicBlock const* predecessor) {
    basicBlock->removePredecessor(predecessor);
    auto* const firstPhi = dyncast<Phi const*>(&basicBlock->front());
    size_t const bbPhiIndex =
        firstPhi ? firstPhi->indexOf(predecessor) : size_t(-1);
    if (basicBlock->hasSinglePredecessor()) {
        /// Transform all phi nodes into the value at the other predecessor.
        for (auto& phi: basicBlock->phiNodes()) {
            Value* const value = phi.argumentAt(1 - bbPhiIndex).value;
            phi.replaceAllUsesWith(value);
        }
        basicBlock->eraseAllPhiNodes();
    }
    else {
        /// Remove \p *predecessor from all phi nodes
        for (auto& phi: basicBlock->phiNodes()) {
            phi.removeArgument(bbPhiIndex);
        }
    }
}

BasicBlock* opt::splitEdge(std::string name,
                           Context& ctx,
                           BasicBlock* from,
                           BasicBlock* to) {
    auto* tmp = new BasicBlock(ctx, std::move(name));
    auto* function = from->parent();
    function->insert(to, tmp);
    tmp->pushBack(new Goto(ctx, to));
    from->terminator()->updateTarget(to, tmp);
    to->updatePredecessor(from, tmp);
    tmp->addPredecessor(from);
    return tmp;
}

BasicBlock* opt::splitEdge(Context& ctx, BasicBlock* from, BasicBlock* to) {
    return splitEdge("tmp", ctx, from, to);
}

bool opt::splitCriticalEdges(Context& ctx, Function& function) {
    struct DFS {
        Context& ctx;
        utl::hashset<BasicBlock*> visited = {};
        bool modified = false;

        void search(BasicBlock* BB) {
            if (!visited.insert(BB).second) {
                return;
            }
            for (auto* succ: BB->successors()) {
                if (isCriticalEdge(BB, succ)) {
                    splitEdge(ctx, BB, succ);
                    modified = true;
                }
                search(succ);
            }
        }
    };
    DFS dfs{ ctx };
    dfs.search(&function.entry());
    if (dfs.modified) {
        function.invalidateCFGInfo();
    }
    return dfs.modified;
}

ir::BasicBlock* opt::addJoiningPredecessor(
    ir::Context& ctx,
    ir::BasicBlock* header,
    std::span<ir::BasicBlock* const> preds,
    std::string name) {
    // clang-format off
    SC_ASSERT(ranges::all_of(preds, [&](auto* pred) {
        return ranges::contains(pred->successors(), header);
    }), "preds must be predecessors of BB"); // clang-format on
    auto& function = *header->parent();
    auto* preheader = new BasicBlock(ctx, name);
    function.insert(header, preheader);
    for (auto& phi: header->phiNodes()) {
        utl::small_vector<PhiMapping> args;
        for (auto* pred: preds) {
            args.push_back({ pred, phi.operandOf(pred) });
        }
        auto* preheaderPhi = new Phi(args, std::string(phi.name()));
        preheader->pushBack(preheaderPhi);
        phi.addArgument(preheader, preheaderPhi);
    }
    for (auto* pred: preds) {
        pred->terminator()->updateTarget(header, preheader);
        header->removePredecessor(pred);
    }
    preheader->setPredecessors(preds);
    preheader->pushBack(new Goto(ctx, header));
    header->addPredecessor(preheader);
    return preheader;
}

bool opt::hasSideEffects(Instruction const* inst) {
    if (auto* call = dyncast<Call const*>(inst)) {
        return !call->function()->hasAttribute(
            FunctionAttribute::Memory_WriteNone);
    }
    if (isa<Store>(inst)) {
        return true;
    }
    if (isa<TerminatorInst>(inst)) {
        return true;
    }
    return false;
}
