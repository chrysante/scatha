#include "IR/Loop.h"

#include <iostream>

#include <range/v3/view.hpp>
#include <utl/graph.hpp>

#include "Basic/PrintUtil.h"
#include "IR/CFG.h"
#include "IR/Dominance.h"

using namespace scatha;
using namespace ir;

LoopNestingForest LoopNestingForest::compute(ir::Function& function,
                                             DomTree const& domtree) {
    LoopNestingForest result;
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
    impl(impl, &result._virtualRoot, bbs);
    return result;
}

using LNFNode = LoopNestingForest::Node;

namespace {

struct PrintCtx {
    PrintCtx(std::ostream& str): str(str) {}

    void print(LNFNode const* node);

    std::ostream& str;
    Indenter indent{ 2 };
};

} // namespace

void ir::print(LoopNestingForest const& LNF) { ir::print(LNF, std::cout); }

void ir::print(LoopNestingForest const& LNF, std::ostream& str) {
    PrintCtx ctx(str);
    for (auto* root: LNF.roots()) {
        ctx.print(root);
    }
}

void PrintCtx::print(LNFNode const* node) {
    auto* bb = node->basicBlock();
    str << indent << bb->name() << ":\n";
    indent.increase();
    for (auto* child: node->children()) {
        print(child);
    }
    indent.decrease();
}
