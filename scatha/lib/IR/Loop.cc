#include "IR/Loop.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/graph.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "Common/PrintUtil.h"
#include "Common/Ranges.h"
#include "Common/TreeFormatter.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Dominance.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace ranges::views;

/// Induction variables are of the following kind:
/// ```
/// X_0 = phi(x_1, ...)
/// x_1 = x_0 op C
/// ```
/// `x_1` is an induction variable if the following conditions are satisfied:
/// - `C` is a constant
/// - `x_0` and `x_1` are both defined within the loop
/// - `x_1` is computed in every loop iteration, i.e. it post dominates the loop
///   header
static bool isInductionVar(Instruction const* inst, LoopInfo const& loop,
                           DominanceInfo const& postDomInfo) {
    auto* indVar = dyncast<ArithmeticInst const*>(inst);
    if (!indVar) {
        return false;
    }
    using enum ArithmeticOperation;
    /// We can assume the constant to be on the right hand side because
    /// instcombine puts constants there for commutative operations
    if (!isa<Constant>(indVar->rhs())) {
        return false;
    }
    auto* phi = dyncast<Phi const*>(indVar->lhs());
    if (!phi || !loop.isInner(phi->parent())) {
        return false;
    }
    if (!ranges::contains(phi->operands(), indVar)) {
        return false;
    }
    if (indVar->parent() == loop.header()) {
        return true;
    }
    if (!loop.isExiting(loop.header())) {
        return postDomInfo.dominatorSet(loop.header())
            .contains(indVar->parent());
    }
    SC_ASSERT(loop.header()->numSuccessors() <= 2,
              "This won't work with more than two successors");
    auto headerSuccs = loop.header()->successors();
    auto nextItr = ranges::find_if(headerSuccs, [&](auto* succ) {
        return loop.isInner(succ);
    });
    SC_ASSERT(nextItr != headerSuccs.end(),
              "Loop header must have at one successor in the loop");
    return postDomInfo.dominatorSet(*nextItr).contains(indVar->parent());
}

LoopInfo LoopInfo::Compute(LNFNode const& header) {
    if (!header.isProperLoop()) {
        return {};
    }
    LoopInfo loop;
    /// Set the header
    loop._header = header.basicBlock();
    SC_EXPECT(loop._header);
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
    /// Determine the loop closing phi nodes
    for (auto* BB: loop.innerBlocks()) {
        for (auto& inst: *BB) {
            for (auto* phi: inst.users() | Filter<Phi>) {
                if (loop.isExit(phi->parent())) {
                    loop._loopClosingPhiNodes[{ phi->parent(), &inst }] = phi;
                }
            }
        }
    }
    return loop;
}

Function* LoopInfo::function() const { return header()->parent(); }

Phi* LoopInfo::loopClosingPhiNode(BasicBlock const* exit,
                                  Instruction const* loopInst) const {
    SC_EXPECT(isExit(exit));
    auto itr = _loopClosingPhiNodes.find({ exit, loopInst });
    if (itr != _loopClosingPhiNodes.end()) {
        return itr->second;
    }
    return nullptr;
}

opt::ScevExpr const* LoopInfo::getScevExpr(Instruction* inst) const {
    auto itr = scevExprMap.find(inst);
    return itr != scevExprMap.end() ? itr->second.get() : nullptr;
}

opt::ScevExpr* LoopInfo::setScevExpr(Instruction* inst,
                                     UniquePtr<opt::ScevExpr> expr) {
    auto* result = expr.get();
    scevExprMap.insert_or_assign(inst, std::move(expr));
    return result;
}

static void printImpl(LoopInfo const& loop, std::ostream& str,
                      TreeFormatter& formatter) {
    formatter.push(Level::Child);
    str << formatter.beginLine() << "Header: " << loop.header()->name()
        << std::endl;
    formatter.pop();
    auto list = [&](std::string_view name, auto&& elems, bool last = false) {
        formatter.push(last ? Level::LastChild : Level::Child);
        str << formatter.beginLine() << name << ":" << std::endl;
        size_t size = elems.size();
        for (auto [index, elem]: elems | enumerate) {
            formatter.push(index == size - 1 ? Level::LastChild : Level::Child);
            str << formatter.beginLine() << elem << std::endl;
            formatter.pop();
        }
        formatter.pop();
    };
    static constexpr auto ToName = transform(&Value::name);
    list("Inner blocks", loop.innerBlocks() | ToName);
    list("Entering blocks", loop.enteringBlocks() | ToName);
    list("Latches", loop.latches() | ToName);
    list("Exiting blocks", loop.exitingBlocks() | ToName);
    list("Exit blocks", loop.exitBlocks() | ToName);
    list("Loop closing phi nodes",
         loop.loopClosingPhiMap() | transform([](auto& elem) {
        auto [key, phi] = elem;
        auto [exit, inst] = key;
        return utl::strcat("{ ", exit->name(), ", ", inst->name(), " } -> ",
                           phi->name());
    }));
    list("Induction variables", loop.inductionVariables() | ToName,
         /* last = */ true);
}

void ir::print(LoopInfo const& loop, std::ostream& str) {
    TreeFormatter formatter;
    printImpl(loop, str, formatter);
}

void ir::print(LoopInfo const& loop) { print(loop, std::cout); }

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

bool ir::makeLCSSA(Function& function) {
    auto& LNF = function.getOrComputeLNF();
    bool modified = false;
    LNF.postorderDFS([&](LNFNode* node) {
        if (node->isProperLoop()) {
            auto& loop = node->loopInfo();
            modified |= makeLCSSA(loop);
        }
    });
    return modified;
}

static bool makeLCSSAPass(Context&, Function& F) { return makeLCSSA(F); }

SC_REGISTER_FUNCTION_PASS(makeLCSSAPass, "lcssa",
                          PassCategory::Canonicalization, {});

static BasicBlock* getIdom(BasicBlock* dominator, BasicBlock* BB,
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
    utl::hashmap<BasicBlock*, Phi*> exitToPhiMap;

    LCSSAContext(Instruction* inst, LoopInfo& loop):
        inst(inst), loop(loop), function(*loop.function()) {}

    /// \Returns the block through which the loop must exit to get to \p user
    BasicBlock* getExitingBlock(Instruction* user) const {
        auto* P = [&] {
            if (auto* phi = dyncast<Phi*>(user)) {
                return phi->predecessorOf(inst);
            }
            return user->parent();
        }();
        return getIdom(inst->parent(), P,
                       [&](auto* block) { return loop.isExiting(block); });
    }

    BasicBlock* getExitBlock(Instruction* user) const {
        BasicBlock* parent = user->parent();
        if (auto* phi = dyncast<Phi*>(user)) {
            if (loop.isExit(parent)) {
                return parent;
            }
            parent = phi->predecessorOf(inst);
        }
        return getIdom(inst->parent(), parent,
                       [&](auto* block) { return loop.isExit(block); });
    }

    auto getExitPhi(Instruction* user) {
        auto* exit = getExitBlock(user);
        auto itr = exitToPhiMap.find(exit);
        if (itr != exitToPhiMap.end()) {
            return itr->second;
        }
        utl::small_vector<PhiMapping> phiArgs;
        for (auto* pred: exit->predecessors()) {
            /// Not sure if this must be asserted. Maybe we can use undef if
            /// this is false
            SC_ASSERT(loop.isExiting(pred), "");
            phiArgs.push_back({ pred, inst });
        }
        Phi* phi = new Phi(phiArgs, utl::strcat(inst->name(), ".phi"));
        exit->insert(exit->phiEnd(), phi);
        exitToPhiMap.insert({ exit, phi });
        return phi;
    };

    bool run() {
        bool modified = false;
        for (auto* user: inst->users() | ToSmallVector<>) {
            auto* P = user->parent();
            if (loop.isInner(P)) {
                continue;
            }
            if (isa<Phi>(user) && loop.isExit(P)) {
                auto* phi = cast<Phi*>(user);
                auto* exit = getExitBlock(user);
                exitToPhiMap[exit] = phi;
                continue;
            }
            auto* phi = getExitPhi(user);
            user->updateOperand(inst, phi);
            modified = true;
        }
        return modified;
    }
};

} // namespace

bool ir::makeLCSSA(LoopInfo& loop) {
    bool modified = false;
    for (auto* BB: loop.innerBlocks()) {
        for (auto& inst: *BB) {
            LCSSAContext context(&inst, loop);
            modified |= context.run();
            for (auto [exit, phi]: context.exitToPhiMap) {
                loop._loopClosingPhiNodes[{ exit, &inst }] = phi;
            }
        }
    }
    return modified;
}

static bool needPreheader(std::span<BasicBlock* const> preds) {
    if (preds.size() == 1) {
        return preds.front()->successors().size() > 1;
    }
    return !preds.empty();
}

static bool needLatch(std::span<BasicBlock* const> preds) {
    return preds.size() > 1;
}

static utl::hashset<BasicBlock*> gatherLoopBlocks(LNFNode& loop) {
    utl::hashset<BasicBlock*> nodes;
    loop.preorderDFS([&](auto* node) { nodes.insert(node->basicBlock()); });
    return nodes;
}

bool ir::simplifyLoop(Context& ctx, LNFNode& loop) {
    auto* header = loop.basicBlock();
    auto& LNF = loop.getLNF();
    utl::small_vector<BasicBlock*> loopPreds, nonLoopPreds;
    for (auto* pred: header->predecessors()) {
        auto* predNode = LNF[pred];
        if (predNode->isLoopNodeOf(&loop)) {
            loopPreds.push_back(pred);
        }
        else {
            nonLoopPreds.push_back(pred);
        }
    }
    bool modified = false;
    // Preheader
    if (needPreheader(nonLoopPreds)) {
        auto* pred =
            opt::addJoiningPredecessor(ctx, header, nonLoopPreds, "preheader");
        LNF.addNode(loop.parent(), pred);
        modified = true;
    }
    // Single latch
    if (needLatch(loopPreds)) {
        auto* pred =
            opt::addJoiningPredecessor(ctx, header, loopPreds, "latch");
        LNF.addNode(&loop, pred);
        modified = true;
    }
    // Dedicated exits
    utl::hashmap<BasicBlock*, utl::small_vector<BasicBlock*>> exitMap;
    auto loopNodes = gatherLoopBlocks(loop);
    loop.preorderDFS([&](LNFNode* node) {
        auto* BB = node->basicBlock();
        for (auto* exit: BB->successors()) {
            if (loopNodes.contains(exit)) {
                continue;
            }
            bool hasNonLoopPreds =
                ranges::any_of(exit->predecessors(), [&](auto* pred) {
                return !loopNodes.contains(pred);
            });
            if (hasNonLoopPreds) {
                exitMap[exit].push_back(BB);
            }
        }
    });
    for (auto& [exit, loopPreds]: exitMap) {
        auto* pred = opt::addJoiningPredecessor(ctx, exit, loopPreds, "exit");
        LNF.addNode(loop.parent(), pred);
        modified = true;
    }
    if (modified) {
        auto& F = *header->parent();
        F.invalidateDomInfo();
        loop.invalidateLoopInfo();
    }
    return modified;
}

bool LNFNode::isProperLoop() const {
    if (!parent()) {
        return false;
    }
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

LoopNestingForest const& LNFNode::getLNF() const {
    auto* node = this;
    while (node->parent()) {
        node = node->parent();
    }
    return static_cast<LoopNestingForest const&>(*node);
}

std::unique_ptr<LoopNestingForest> LoopNestingForest::compute(
    ir::Function& function, DomTree const& domtree) {
    std::unique_ptr<LoopNestingForest> result(new LoopNestingForest());
    auto BBs = function | TakeAddress | ranges::to<utl::hashset<BasicBlock*>>;
    result->_nodes = BBs | transform([](auto* bb) { return Node(bb); }) |
                     ranges::to<NodeSet>;
    auto dfs = [&](auto& dfs, Node* root,
                   utl::hashset<BasicBlock*> const& BBs) -> void {
        utl::small_vector<utl::hashset<BasicBlock*>, 4> SCCs;
        auto succs = [&](BasicBlock* bb) {
            return bb->successors() |
                   filter([&](auto* succ) { return BBs.contains(succ); });
        };
        auto beginSCC = [&] { SCCs.emplace_back(); };
        auto emitVertex = [&](BasicBlock* bb) { SCCs.back().insert(bb); };
        utl::compute_sccs(BBs.begin(), BBs.end(), succs, beginSCC, emitVertex);
        for (auto& scc: SCCs) {
            SC_ASSERT(!scc.empty(), "Empty SCCs cannot exist");
            auto* header = *scc.begin();
            while (true) {
                auto* dom = domtree.idom(header);
                if (!scc.contains(dom)) {
                    break;
                }
                header = dom;
            }
            auto* headerNode = result->findMut(header);
            root->addChild(headerNode);
            scc.erase(header);
            dfs(dfs, headerNode, scc);
        }
    };
    dfs(dfs, result.get(), BBs);
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

bool ir::compareEqual(
    LoopNestingForest const& A, LoopNestingForest const& B,
    utl::function_view<void(LNFNode const&, LNFNode const&)> DC) {
    auto invokeDC = [&](LNFNode const& nodeA, LNFNode const& nodeB) {
        if (DC) {
            DC(nodeA, nodeB);
        }
    };
    auto sortedChildren = [](LNFNode const& node) {
        auto children = node.children() | ToSmallVector<>;
        ranges::sort(children, ranges::less{}, &LNFNode::basicBlock);
        return children;
    };
    auto DFS = [&](auto& DFS, LNFNode const& A, LNFNode const& B) -> bool {
        if (A.basicBlock() != B.basicBlock() ||
            A.children().size() != B.children().size())
        {
            invokeDC(A, B);
            return false;
        }
        auto CA = sortedChildren(A);
        auto CB = sortedChildren(B);
        for (auto [cA, cB]: zip(CA, CB)) {
            if (!DFS(DFS, *cA, *cB)) {
                return false;
            }
        }
        return true;
    };
    return DFS(DFS, *A.virtualRoot(), *B.virtualRoot());
}

namespace {

struct LNFPrintCtx {
    std::ostream& str;
    bool printLoopInfo = false;
    TreeFormatter formatter = {};

    void run(LoopNestingForest const& LNF) {
        for (auto [index, root]: LNF.roots() | enumerate) {
            print(root, index == LNF.roots().size() - 1);
        }
    }

    void print(LNFNode const* node, bool lastInParent) {
        formatter.push(!lastInParent ? Level::Child : Level::LastChild);
        str << formatter.beginLine();
        auto* BB = node->basicBlock();
        bool isProper = node->isProperLoop();
        tfmt::formatScope(isProper ? tfmt::Bold : tfmt::None, str, [&] {
            (BB ? str << formatName(*BB) : str << "NULL") << std::endl;
        });
        if (isProper && printLoopInfo) {
            formatter.push(node->children().empty() ? Level::LastChild :
                                                      Level::Child);
            str << formatter.beginLine()
                << tfmt::format(tfmt::BrightBlue, "Loop Info:") << "\n";
            printImpl(node->loopInfo(), str, formatter);
            formatter.pop();
        }
        for (auto [index, child]: node->children() | enumerate) {
            print(child, index == node->children().size() - 1);
        }
        formatter.pop();
    }
};

} // namespace

void ir::print(LoopNestingForest const& LNF) { ir::print(LNF, std::cout); }

void ir::print(LoopNestingForest const& LNF, std::ostream& str) {
    LNFPrintCtx{ str }.run(LNF);
}

static bool printLNFPass(Context&, Function& F, PassArgumentMap const& args) {
    auto& LNF = F.getOrComputeLNF();
    LNFPrintCtx{ std::cout, .printLoopInfo = args.get<bool>("info") }.run(LNF);
    return false;
}

SC_REGISTER_FUNCTION_PASS(printLNFPass, "print-lnf", PassCategory::Other,
                          { Flag{ "info", false } });
