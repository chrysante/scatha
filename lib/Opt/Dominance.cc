#include "Opt/Dominance.h"

#include <iostream>

#include <range/v3/numeric.hpp>
#include <range/v3/to_container.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Basic/PrintUtil.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace opt;
using namespace ir;

DomTree::DomTree() = default;

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

DomTree opt::buildDomTree(ir::Function& function) {
    DomTree result;
    auto const nodeSet = [&] {
        utl::hashset<BasicBlock*> res;
        for (auto& bb: function) {
            res.insert(&bb);
        }
        return res;
    }();
    auto domSets = [&] {
        utl::hashmap<BasicBlock*, utl::hashset<BasicBlock*>> res;
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
            ranges::views::transform([&](auto* pred) -> auto const& {
                return domSets[pred];
            });
        merge(newDomSet, intersect(predDomSets));
        auto& oldDomSet = domSets[bb];
        if (newDomSet != oldDomSet) {
            oldDomSet = newDomSet;
            for (auto* succ: bb->successors()) {
                worklist.insert(succ);
            }
        }
    }
    for (auto&& [bb, domSet]: domSets) {
        std::cout << bb->name() << ": [";
        for (bool first = true; auto* dom: domSet) {
            std::cout << (first ? first = false, "" : ", ") << dom->name();
        }
        std::cout << "]\n";
    }
    result._nodes = nodeSet | ranges::views::transform([](BasicBlock* bb) {
                        return DomTree::Node{ bb };
                    }) |
                    ranges::to<DomTree::NodeSet>;
    result._root = &result.findMut(&function.entry());
    for (auto& start: result._nodes) {
        utl::hashset<DomTree::Node*> visited;
        auto const& domSet = domSets[start.basicBlock()];
        auto findParent    = [&](DomTree::Node& node,
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
                res->_children.push_back(mutStart);
                mutStart->_parent = res;
                break;
            }
        }
    }
    return result;
}

void opt::print(DomTree const& domTree) {
    print(domTree, std::cout);
}

namespace {

struct PrintCtx {
    PrintCtx(std::ostream& str): str(str) {}

    void print(DomTree::Node const& node);

    std::ostream& str;
    Indenter indent;
};

} // namespace

void opt::print(DomTree const& domTree, std::ostream& str) {
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

using DFResult = utl::hashmap<ir::BasicBlock*, utl::small_vector<ir::BasicBlock*>>;

namespace {

struct DFContext {
    DFContext(ir::Function& function, DomTree const& domTree, DFResult& result):
        function(function), domTree(domTree), result(result) {}
    
    void compute(DomTree::Node const& node);
    
    ir::Function& function;
    DomTree const& domTree;
    DFResult& result;
};

} // namespace

DFResult opt::computeDominanceFrontiers(ir::Function& function,
                                        DomTree const& domTree) {
    DFResult result;
    DFContext ctx(function, domTree, result);
    ctx.compute(domTree.root());
    return result;
}

void DFContext::compute(DomTree::Node const& node) {
    for (auto& child: node.children()) {
        compute(child);
    }
    auto* bb = node.basicBlock();
    auto& df = result[bb];
    // DF_local
    for (BasicBlock* succ: bb->successors()) {
        auto& succNode = domTree[succ];
        if (succNode.parent() != &node) {
            df.push_back(succ);
        }
    }
    // DF_up
    for (DomTree::Node const& child: node.children()) {
        for (auto* v: result[child.basicBlock()]) {
            auto& vNode = domTree[v];
            if (vNode.parent() != &node) {
                df.push_back(v);
            }
        }
    }
}
