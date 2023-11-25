#include "IR/Loop.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/graph.hpp>
#include <utl/strcat.hpp>

#include "Common/PrintUtil.h"
#include "Common/Ranges.h"
#include "Common/TreeFormatter.h"
#include "IR/CFG.h"
#include "IR/Dominance.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace ir;

/// Induction variables are of the following kind:
/// ```
/// X_0 = phi(x_1, ...)
/// x_1 = x_0 +- C
/// ```
/// `x_1` is an induction variable if the following conditions are satisfied:
/// - `C` is a constant
/// - `x_0` and `x_1` are both defined within the loop
/// - `x_1` is computed in every loop iteration, i.e. it post dominates the loop
///   header
static bool isInductionVar(Instruction const* inst,
                           LoopInfo const& loop,
                           DominanceInfo const& postDomInfo) {
    auto* add = dyncast<ArithmeticInst const*>(inst);
    if (!add) {
        return false;
    }
    using enum ArithmeticOperation;
    if (add->operation() != Add && add->operation() != Sub) {
        return false;
    }
    /// We can assume the constant to be on the right hand side because
    /// instcombine puts constants there for commutative operations
    if (!isa<Constant>(add->rhs())) {
        return false;
    }
    auto* phi = dyncast<Phi const*>(add->lhs());
    if (!phi || !loop.isInner(phi->parent())) {
        return false;
    }
    if (!ranges::contains(phi->operands(), add)) {
        return false;
    }
    if (!postDomInfo.dominatorSet(loop.header()).contains(add->parent())) {
        return false;
    }
    return true;
}

LoopInfo LoopInfo::Compute(LNFNode const& header) {
    LoopInfo loop;
    /// Set the header
    loop._header = header.basicBlock();
    /// Gather all inner blocks
    header.preorderDFS([&](LNFNode const* node) {
        loop._innerBlocks.insert(node->basicBlock());
    });
    /// Determine exiting and exit blocks and induction variables
    auto const& postDomInfo = loop.function()->getOrComputePostDomInfo();
    for (auto* BB: loop.innerBlocks()) {
        if (isa<Branch>(BB->terminator())) {
            for (auto* succ: BB->successors()) {
                if (!loop.isInner(succ)) {
                    loop._exitingBlocks.insert(BB);
                    loop._exitBlocks.insert(succ);
                }
            }
        }
        for (auto& inst: *BB) {
            if (isInductionVar(&inst, loop, postDomInfo)) {
                loop._inductionVars.push_back(&inst);
            }
        }
    }
    /// Determine entering blocks and latches
    for (auto* pred: loop.header()->predecessors()) {
        if (loop.isInner(pred)) {
            loop._latches.insert(pred);
        }
        else {
            loop._enteringBlocks.insert(pred);
        }
    }
    return loop;
}

Function* LoopInfo::function() const { return header()->parent(); }

std::span<Phi* const> LoopInfo::loopClosingPhiNodes(
    BasicBlock const* exit) const {
    SC_EXPECT(isExit(exit));
    auto itr = _loopClosingPhiNodes.find(exit);
    if (itr != _loopClosingPhiNodes.end()) {
        return itr->second;
    }
    return {};
}

bool ir::isLCSSA(LoopInfo const& loop) {
    for (auto* BB: loop.innerBlocks()) {
        for (auto& inst: *BB) {
            for (auto* user: inst.users()) {
                auto* P = user->parent();
                if (loop.isInner(P)) {
                    continue;
                }
                if (isa<Phi>(user) && loop.isExit(P)) {
                    continue;
                }
                return false;
            }
        }
    }
    return true;
}

void ir::makeLCSSA(Function& function) {
    auto& LNF = function.getOrComputeLNF();
    LNF.postorderDFS([&](LNFNode* node) {
        if (node->isProperLoop()) {
            auto& loop = node->loopInfo();
            makeLCSSA(loop);
        }
    });
}

SC_REGISTER_PASS(
    [](Context&, Function& F) {
    makeLCSSA(F);
    return true;
    },
    "lcssa",
    PassCategory::Canonicalization);

static BasicBlock* getIdom(BasicBlock* dominator,
                           BasicBlock* BB,
                           auto condition) {
    auto* F = dominator->parent();
    auto& domTree = F->getOrComputeDomTree();
    auto* node = domTree[BB];
    while (!condition(node->basicBlock())) {
        node = node->parent();
    }
    return node->basicBlock();
}

namespace {

struct LCSSAContext {
    /// The instruction for which we are adding phi nodes
    Instruction* inst;
    LoopInfo& loop;
    Function& function;

    /// Maps exit blocks to their phi node for this instruction
    utl::hashmap<BasicBlock*, Phi*> exitPhis;

    LCSSAContext(Instruction* inst, LoopInfo& loop):
        inst(inst), loop(loop), function(*loop.function()) {}

    /// \Returns the block through which the loop must exit to get to \p user
    BasicBlock* getExitBlock(Instruction* user) const {
        auto* P = [&] {
            if (auto* phi = dyncast<Phi*>(user)) {
                return phi->predecessorOf(inst);
            }
            return user->parent();
        }();
        return getIdom(inst->parent(), P, [&](auto* block) {
            return loop.isExit(block);
        });
    }

    auto getExitPhi(Instruction* user) {
        auto* exit = getExitBlock(user);
        auto itr = exitPhis.find(exit);
        if (itr != exitPhis.end()) {
            return itr->second;
        }
        auto* pred = exit->singlePredecessor();
        SC_ASSERT(pred,
                  "This may not be true but we just assert it for now "
                  "until we have a better solution");
        Phi* phi =
            new Phi({ { pred, inst } }, utl::strcat(inst->name(), ".phi"));
        exit->insert(exit->phiEnd(), phi);
        exitPhis.insert({ exit, phi });
        return phi;
    };

    void run() {
        for (auto* user: inst->users() | ToSmallVector<>) {
            auto* P = user->parent();
            if (loop.isInner(P)) {
                continue;
            }
            if (isa<Phi>(user) && loop.isExit(P)) {
                auto* phi = cast<Phi*>(user);
                if (phi->numOperands() == 1) {
                    auto* exit = getExitBlock(user);
                    exitPhis[exit] = phi;
                }
                continue;
            }
            auto* phi = getExitPhi(user);
            user->updateOperand(inst, phi);
        }
    }
};

} // namespace

void ir::makeLCSSA(LoopInfo& loop) {
    for (auto* BB: loop.innerBlocks()) {
        for (auto& inst: *BB) {
            LCSSAContext context(&inst, loop);
            context.run();
            for (auto [exit, phi]: context.exitPhis) {
                loop._loopClosingPhiNodes[exit].push_back(phi);
            }
        }
    }
}

bool LNFNode::isProperLoop() const {
    if (!children().empty()) {
        return true;
    }
    return ranges::contains(basicBlock()->predecessors(), basicBlock());
}

bool LNFNode::isLoopNodeOf(LNFNode const* header) const {
    auto* node = this;
    while (node != nullptr) {
        if (node == header) {
            return true;
        }
        node = node->parent();
    }
    return false;
}

LoopNestingForest LoopNestingForest::compute(ir::Function& function,
                                             DomTree const& domtree) {
    LoopNestingForest result;
    result._virtualRoot = std::make_unique<Node>();
    auto bbs = function | TakeAddress | ranges::to<utl::hashset<BasicBlock*>>;
    result._nodes =
        bbs | ranges::views::transform([](auto* bb) { return Node(bb); }) |
        ranges::to<NodeSet>;
    auto impl = [&domtree,
                 &result](auto& impl,
                          Node* root,
                          utl::hashset<BasicBlock*> const& bbs) -> void {
        utl::small_vector<utl::hashset<BasicBlock*>, 4> sccs;
        utl::compute_sccs(
            bbs.begin(),
            bbs.end(),
            [&](BasicBlock* bb) {
            return bb->successors() | ranges::views::filter([&](auto* succ) {
                       return bbs.contains(succ);
                   });
            },
            [&] { sccs.emplace_back(); },
            [&](BasicBlock* bb) { sccs.back().insert(bb); });
        for (auto& scc: sccs) {
            auto* header = *scc.begin();
            while (true) {
                auto* dom = domtree.idom(header);
                if (!scc.contains(dom)) {
                    break;
                }
                header = dom;
            }
            auto* headerNode = result.findMut(header);
            root->addChild(headerNode);
            scc.erase(header);
            impl(impl, headerNode, scc);
        }
    };
    impl(impl, result._virtualRoot.get(), bbs);
    return result;
}

void LoopNestingForest::addNode(Node const* parent, BasicBlock* BB) {
    auto [itr, success] = _nodes.insert(Node(BB));
    SC_ASSERT(success, "BB is already in the tree");
    auto* node = const_cast<Node*>(&*itr);
    const_cast<Node*>(parent)->addChild(node);
}

void LoopNestingForest::addNode(BasicBlock const* parent, BasicBlock* BB) {
    addNode((*this)[parent], BB);
}

namespace {

struct LNFPrintCtx {
    std::ostream& str;
    TreeFormatter formatter;

    explicit LNFPrintCtx(std::ostream& str): str(str) {}

    void print(LNFNode const* node, bool lastInParent) {
        formatter.push(!lastInParent ? Level::Child : Level::LastChild);
        str << formatter.beginLine();
        auto* BB = node->basicBlock();
        bool isNonTrivialLoop = node->children().size() > 0;
        tfmt::format(isNonTrivialLoop ? tfmt::Bold : tfmt::None, str, [&] {
            str << (BB ? BB->name() : "NULL") << std::endl;
        });
        for (auto [index, child]: node->children() | ranges::views::enumerate) {
            print(child, index == node->children().size() - 1);
        }
        formatter.pop();
    }
};

} // namespace

void ir::print(LoopNestingForest const& LNF) { ir::print(LNF, std::cout); }

void ir::print(LoopNestingForest const& LNF, std::ostream& str) {
    LNFPrintCtx ctx(str);
    for (auto [index, root]: LNF.roots() | ranges::views::enumerate) {
        ctx.print(root, index == LNF.roots().size() - 1);
    }
}
