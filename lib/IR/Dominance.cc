#include "IR/Dominance.h"

#include <array>
#include <iostream>

#include <range/v3/numeric.hpp>
#include <range/v3/to_container.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/PrintUtil.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

using DomFrontMap = DominanceInfo::DomFrontMap;

static void merge(utl::hashset<BasicBlock*>& dest,
                  utl::hashset<BasicBlock*> const& source) {
    for (auto* bb: source) {
        dest.insert(bb);
    }
}

static utl::hashset<BasicBlock*> intersect(
    utl::hashset<BasicBlock*> lhs, utl::hashset<BasicBlock*> const& rhs) {
    for (auto itr = lhs.begin(); itr != lhs.end();) {
        if (rhs.contains(*itr)) {
            ++itr;
        }
        else {
            itr = lhs.erase(itr);
        }
    }
    return lhs;
}

static utl::hashset<BasicBlock*> intersect(auto&& range) {
    if (range.empty()) {
        return {};
    }
    return ranges::accumulate(std::next(ranges::begin(range)),
                              ranges::end(range),
                              *range.begin(),
                              [](auto&& a, auto const& b) {
        return intersect(std::move(a), b);
    });
}

static utl::small_vector<BasicBlock*> exitNodes(Function& function) {
    return function | ranges::views::filter([](auto& bb) {
               return isa<Return>(bb.terminator());
           }) |
           TakeAddress | ToSmallVector<>;
}

namespace {

struct PrintCtx {
    PrintCtx(std::ostream& str): str(str) {}

    void print(DomTree::Node const& node);

    std::ostream& str;
    Indenter indent;
};

} // namespace

void ir::print(DomTree const& domTree) { print(domTree, std::cout); }

void ir::print(DomTree const& domTree, std::ostream& str) {
    PrintCtx ctx(str);
    ctx.print(*domTree.root());
}

void PrintCtx::print(DomTree::Node const& node) {
    auto* bb = node.basicBlock();
    std::string_view name = bb ? bb->name() : "<virtual root>";
    str << indent << name << ":\n";
    indent.increase();
    for (auto* child: node.children()) {
        print(*child);
    }
    indent.decrease();
}

DominanceInfo DominanceInfo::compute(Function& function) {
    DominanceInfo result;
    result._dominatorMap = computeDominatorSets(function);
    result._domTree = computeDomTree(function, result._dominatorMap);
    result._domFront = computeDomFronts(function, result._domTree);
    return result;
}

DominanceInfo DominanceInfo::computePost(Function& function) {
    DominanceInfo result;
    result._dominatorMap = computePostDomSets(function);
    result._domTree = computePostDomTree(function, result._dominatorMap);
    result._domFront = computePostDomFronts(function, result._domTree);
    return result;
}

utl::hashset<BasicBlock*> const& DominanceInfo::dominatorSet(
    ir::BasicBlock const* basicBlock) const {
    auto itr = _dominatorMap.find(basicBlock);
    SC_ASSERT(itr != _dominatorMap.end(), "Basic block not found");
    return itr->second;
}

std::span<BasicBlock* const> DominanceInfo::domFront(
    ir::BasicBlock const* basicBlock) const {
    auto itr = _domFront.find(basicBlock);
    if (itr != _domFront.end()) {
        return itr->second;
    }
    /// Basic block not found
    return {};
}

DominanceInfo::DomMap DominanceInfo::computeDomSetsImpl(
    Function& function,
    std::span<BasicBlock* const> entries,
    auto predecessors,
    auto successors) {
    /// https://pages.cs.wisc.edu/~fischer/cs701.f07/lectures/Lecture20.pdf
    auto const nodeSet = [&] {
        utl::hashset<BasicBlock*> res;
        for (auto& bb: function) {
            res.insert(&bb);
        }
        return res;
    }();
    DomMap domSets = [&] {
        DomMap res;
        for (auto& bb: function) {
            res.insert({ &bb, nodeSet });
        }
        return res;
    }();
    SC_ASSERT(
        !entries.empty(),
        "Handle post-dom case without exit notes outside of this function");
    auto worklist = entries | ranges::to<utl::hashset<BasicBlock*>>;
    while (!worklist.empty()) {
        auto* bb = *worklist.begin();
        worklist.erase(worklist.begin());
        utl::hashset<BasicBlock*> newDomSet = { bb };
        auto predDomSets =
            predecessors(bb) |
            ranges::views::transform(
                [&](auto* pred) -> auto const& { return domSets[pred]; });
        merge(newDomSet, intersect(predDomSets));
        auto& oldDomSet = domSets[bb];
        if (newDomSet != oldDomSet) {
            oldDomSet = newDomSet;
            for (auto* succ: successors(bb)) {
                worklist.insert(succ);
            }
        }
    }
    return domSets;
}

DominanceInfo::DomMap DominanceInfo::computeDominatorSets(Function& function) {
    return computeDomSetsImpl(
        function,
        std::array{ &function.entry() },
        [](BasicBlock* bb) { return bb->predecessors(); },
        [](BasicBlock* bb) { return bb->successors(); });
}

DominanceInfo::DomMap DominanceInfo::computePostDomSets(Function& function) {
    /// Same as `computeDominatorSets` but with reversed edges.
    auto exits = ::exitNodes(function);
    if (exits.empty()) {
        return {};
    }
    return computeDomSetsImpl(
        function,
        exits,
        [](BasicBlock* bb) { return bb->successors(); },
        [](BasicBlock* bb) { return bb->predecessors(); });
}

DomTree DominanceInfo::computeDomTreeImpl(ir::Function& function,
                                          DomMap const& domSets,
                                          BasicBlock* entry,
                                          std::span<BasicBlock* const> exits,
                                          auto predecessors) {
    DomTree result;
    auto const nodeSet =
        function | TakeAddress | ranges::to<utl::hashset<BasicBlock*>>;
    result._nodes = nodeSet | ranges::views::transform([](BasicBlock* bb) {
                        return DomTree::Node{ bb };
                    }) |
                    ranges::to<DomTree::NodeSet>;
    if (entry) {
        result._root = result.findMut(entry);
    }
    else {
        auto& constRoot = *result._nodes.insert(DomTree::Node(nullptr)).first;
        result._root = const_cast<DomTree::Node*>(&constRoot);
        for (auto* exit: exits) {
            result._root->addChild(result.findMut(exit));
        }
    }
    for (auto& start: result._nodes) {
        if (!start.basicBlock()) {
            continue;
        }
        auto const& dominatorSet = domSets.find(start.basicBlock())->second;
        utl::hashset<DomTree::Node*> visited = { const_cast<DomTree::Node*>(
            &start) };
        auto findParent = [&](DomTree::Node* node,
                              auto& findParent) -> DomTree::Node* {
            if (visited.contains(node)) {
                return nullptr;
            }
            visited.insert(node);
            if (dominatorSet.contains(node->basicBlock())) {
                return node;
            }
            for (auto* pred: predecessors(node->basicBlock())) {
                auto* predNode = result.findMut(pred);
                if (auto res = findParent(predNode, findParent)) {
                    return res;
                }
            }
            return nullptr;
        };
        for (auto* pred: predecessors(start.basicBlock())) {
            auto* predNode = result.findMut(pred);
            auto* res = findParent(predNode, findParent);
            if (!res) {
                continue;
            }
            auto* mutStart = const_cast<DomTree::Node*>(&start);
            res->addChild(mutStart);
            break;
        }
    }
    return result;
}

DomTree DominanceInfo::computeDomTree(Function& function,
                                      DomMap const& domSets) {
    return computeDomTreeImpl(function,
                              domSets,
                              &function.entry(),
                              {},
                              [](BasicBlock* bb) {
        return bb->predecessors();
    });
}

DomTree DominanceInfo::computePostDomTree(Function& function,
                                          DomMap const& postDomSets) {
    auto exits = ::exitNodes(function);
    /// Can't compute post-dominator tree for function without an exit node.
    if (exits.empty()) {
        return {};
    }
    auto* exitNode = exits.size() == 1 ? exits.front() : nullptr;
    return computeDomTreeImpl(function,
                              postDomSets,
                              exitNode,
                              exits,
                              [](BasicBlock* bb) { return bb->successors(); });
}

DomFrontMap DominanceInfo::computeDomFrontsImpl(ir::Function& function,
                                                DomTree const& domTree,
                                                auto successors) {
    if (domTree.empty()) {
        return {};
    }
    struct DFContext {
        DFContext(Function& function,
                  DomTree const& domTree,
                  DomFrontMap& df,
                  decltype(successors) succ):
            function(function), domTree(domTree), df(df), succ(succ) {}

        void compute(DomTree::Node const* uNode) {
            for (auto* n: uNode->children()) {
                compute(n);
            }
            auto* u = uNode->basicBlock();
            auto& dfU = df[u];
            // DF_local
            for (BasicBlock* v: succ(u)) {
                if (domTree.idom(v) != u) {
                    dfU.push_back(v);
                }
            }
            // DF_up
            for (auto* wNode: uNode->children()) {
                for (auto* v: df[wNode->basicBlock()]) {
                    if (domTree.idom(v) != u) {
                        dfU.push_back(v);
                    }
                }
            }
        }

        Function& function;
        DomTree const& domTree;
        DomFrontMap& df;
        decltype(successors) succ;
    };
    DomFrontMap result;
    DFContext ctx(function, domTree, result, successors);
    ctx.compute(domTree.root());
    return result;
}

DomFrontMap DominanceInfo::computeDomFronts(Function& function,
                                            DomTree const& domTree) {
    return computeDomFrontsImpl(function, domTree, [](BasicBlock* bb) {
        return bb->successors();
    });
}

DomFrontMap DominanceInfo::computePostDomFronts(Function& function,
                                                DomTree const& postDomTree) {
    auto exits = exitNodes(function);
    return computeDomFrontsImpl(function, postDomTree, [&](BasicBlock* bb) {
        return bb ? bb->predecessors() : std::span<BasicBlock* const>(exits);
    });
}

utl::hashset<BasicBlock*> unionDF(BasicBlock* X,
                                  utl::hashset<BasicBlock*> const& blocks,
                                  DomFrontMap const& domFronts) {
    utl::hashset<BasicBlock*> result;
    for (auto* BB: blocks) {
        auto& DF = domFronts.find(BB)->second;
        for (auto* DF_BB: DF) {
            result.insert(DF_BB);
        }
    }
    auto& DF = domFronts.find(X)->second;
    for (auto* DF_BB: DF) {
        result.insert(DF_BB);
    }
    return result;
}

static utl::hashset<BasicBlock*> iterate(BasicBlock* BB,
                                         utl::hashset<BasicBlock*> DF,
                                         DomFrontMap const& domFronts) {
    while (true) {
        auto newDF = unionDF(BB, DF, domFronts);
        if (DF == newDF) {
            return DF;
        }
        DF = std::move(newDF);
    }
}

DomFrontMap DominanceInfo::computeIterDomFronts(DomFrontMap const& domFronts) {
    DomFrontMap result;
    for (auto& [BB, DF]: domFronts) {
        result[BB] =
            iterate(BB, DF | ranges::to<utl::hashset<BasicBlock*>>, domFronts) |
            ToSmallVector<>;
    }
    return result;
}
