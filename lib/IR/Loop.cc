#include "IR/Loop.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/graph.hpp>

#include "Common/PrintUtil.h"
#include "Common/TreeFormatter.h"
#include "IR/CFG.h"
#include "IR/Dominance.h"

using namespace scatha;
using namespace ir;

bool LNFNode::isProperLoop() const {
    if (!children().empty()) {
        return true;
    }
    return ranges::contains(basicBlock()->predecessors(), basicBlock());
}

LoopNestingForest LoopNestingForest::compute(ir::Function& function,
                                             DomTree const& domtree) {
    LoopNestingForest result;
    result._virtualRoot = std::make_unique<Node>();
    auto bbs = function |
               ranges::views::transform([](auto& bb) { return &bb; }) |
               ranges::to<utl::hashset<BasicBlock*>>;
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
