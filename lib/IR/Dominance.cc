#include "IR/Dominance.h"

#include <iostream>

#include <range/v3/numeric.hpp>
#include <range/v3/to_container.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Basic/PrintUtil.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

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
    ctx.print(domTree.root());
}

void PrintCtx::print(DomTree::Node const& node) {
    str << indent << node.basicBlock()->name() << ":\n";
    indent.increase();
    for (auto& child: node.children()) {
        print(child);
    }
    indent.decrease();
}

DominanceInfo DominanceInfo::compute(Function& function) {
    DominanceInfo result;
    result._domMap   = computeDomSets(function);
    result._domTree  = computeDomTree(function, result._domMap);
    result._domFront = computeDomFronts(function, result._domTree);
    return result;
}

utl::hashset<BasicBlock*> const& DominanceInfo::domSet(
    ir::BasicBlock const* basicBlock) const {
    auto itr = _domMap.find(basicBlock);
    SC_ASSERT(itr != _domMap.end(), "Basic block not found");
    return itr->second;
}

std::span<BasicBlock* const> DominanceInfo::domFront(
    ir::BasicBlock const* basicBlock) const {
    auto itr = _domFront.find(basicBlock);
    SC_ASSERT(itr != _domFront.end(), "Basic block not found");
    return itr->second;
}

DominanceInfo::DomMap DominanceInfo::computeDomSets(Function& function) {
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
    utl::hashset<BasicBlock*> worklist = { &function.entry() };
    while (!worklist.empty()) {
        auto* bb = *worklist.begin();
        worklist.erase(worklist.begin());
        utl::hashset<BasicBlock*> newDomSet = { bb };
        auto predDomSets =
            bb->predecessors() |
            ranges::views::transform(
                [&](auto* pred) -> auto const& { return domSets[pred]; });
        merge(newDomSet, intersect(predDomSets));
        auto& oldDomSet = domSets[bb];
        if (newDomSet != oldDomSet) {
            oldDomSet = newDomSet;
            for (auto* succ: bb->successors()) {
                worklist.insert(succ);
            }
        }
    }
    return domSets;
}

DomTree DominanceInfo::computeDomTree(Function& function,
                                      DomMap const& domSets) {
    DomTree result;
    auto const nodeSet = [&] {
        utl::hashset<BasicBlock*> res;
        for (auto& bb: function) {
            res.insert(&bb);
        }
        return res;
    }();
    result._nodes = nodeSet | ranges::views::transform([](BasicBlock* bb) {
                        return DomTree::Node{ bb };
                    }) |
                    ranges::to<DomTree::NodeSet>;
    result._root = &result.findMut(&function.entry());
    for (auto& start: result._nodes) {
        auto const& domSet = domSets.find(start.basicBlock())->second;
        utl::hashset<DomTree::Node*> visited;
        auto findParent = [&](DomTree::Node& node,
                              auto& findParent) -> DomTree::Node* {
            if (visited.contains(&node)) {
                return nullptr;
            }
            visited.insert(&node);
            if (domSet.contains(node.basicBlock())) {
                return &node;
            }
            for (auto* pred: node.basicBlock()->predecessors()) {
                auto& predNode = result.findMut(pred);
                if (auto res = findParent(predNode, findParent)) {
                    return res;
                }
            }
            return nullptr;
        };
        for (auto* pred: start.basicBlock()->predecessors()) {
            auto& predNode = result.findMut(pred);
            if (auto* res = findParent(predNode, findParent)) {
                auto* mutStart = const_cast<DomTree::Node*>(&start);
                res->addChild(mutStart);
                mutStart->setParent(res);
                break;
            }
        }
    }
    return result;
}

DominanceInfo::DomFrontMap DominanceInfo::computeDomFronts(
    Function& function, DomTree const& domTree) {
    struct DFContext {
        DFContext(Function& function, DomTree const& domTree, DomFrontMap& df):
            function(function), domTree(domTree), df(df) {}

        void compute(DomTree::Node const& uNode) {
            for (auto& n: uNode.children()) {
                compute(n);
            }
            auto* u   = uNode.basicBlock();
            auto& dfU = df[u];
            // DF_local
            for (BasicBlock* v: u->successors()) {
                if (domTree.idom(v) != u) {
                    dfU.push_back(v);
                }
            }
            // DF_up
            for (auto& wNode: uNode.children()) {
                for (auto* v: df[wNode.basicBlock()]) {
                    if (domTree.idom(v) != u) {
                        dfU.push_back(v);
                    }
                }
            }
        }

        Function& function;
        DomTree const& domTree;
        DomFrontMap& df;
    };
    DomFrontMap result;
    DFContext ctx(function, domTree, result);
    ctx.compute(domTree.root());
    return result;
}