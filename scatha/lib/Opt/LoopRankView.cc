#include "Opt/LoopRankView.h"

#include <iostream>
#include <queue>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Loop.h"
#include "IR/Print.h"

using namespace scatha;
using namespace ir;
using namespace opt;
using namespace ranges::views;

LoopRankView LoopRankView::Compute(ir::Function& function) {
    return Compute(function, [](auto*) { return true; });
}

namespace {

struct TopSorter {
    Function const& function;
    utl::hashmap<BasicBlock const*, size_t> order;

    explicit TopSorter(Function const& function): function(function) {
        std::queue<BasicBlock const*> queue;
        queue.push(&function.entry());
        utl::hashset<BasicBlock const*> visited = { &function.entry() };
        size_t rank = 0;
        while (!queue.empty()) {
            auto* BB = queue.front();
            queue.pop();
            order[BB] = rank++;
            for (auto* succ: BB->successors()) {
                if (visited.insert(succ).second) {
                    queue.push(succ);
                }
            }
        }
    }

    size_t rank(BasicBlock const* BB) const {
        auto itr = order.find(BB);
        if (itr != order.end()) {
            return order.find(BB)->second;
        }
        return 0;
    }

    auto operator()(std::span<LNFNode const* const> nodes) const {
        auto result = nodes | ToSmallVector<>;
        std::sort(result.begin(), result.end(), [&](auto* A, auto* B) {
            return rank(A->basicBlock()) < rank(B->basicBlock());
        });
        return result;
    }
};

} // namespace

LoopRankView LoopRankView::Compute(
    ir::Function& function,
    utl::function_view<bool(BasicBlock const*)> headerPredicate) {
    /// We collect all the loops of `function` in breadth first search
    /// order of the the loop nesting forest
    LoopRankView LRV;
    auto& LNF = function.getOrComputeLNF();
    auto topsort = TopSorter(function);
    auto DFS = [&](auto& DFS, LNFNode const* header, size_t rank) -> void {
        if (header->isProperLoop() && headerPredicate(header->basicBlock())) {
            /// Loop is better than `resize()` because in 99.9% of cases the
            /// loop will only run once
            while (rank >= LRV._ranks.size()) {
                LRV._ranks.emplace_back();
            }
            LRV._ranks[rank].push_back(header->basicBlock());
        }
        for (auto* node: topsort(header->children())) {
            DFS(DFS, node, rank + 1);
        }
    };
    for (auto* root: topsort(LNF.roots())) {
        DFS(DFS, root, 0);
    }
    return LRV;
}

void opt::print(LoopRankView const& LRV, std::ostream& str) {
    for (auto [index, rank]: LRV | enumerate) {
        str << " - Rank " << index << ":\n    ";
        bool first = true;
        for (auto* BB: rank) {
            if (!first) {
                str << ", ";
            }
            first = false;
            str << format(*BB);
        }
        str << "\n";
    }
}

///
void opt::print(LoopRankView const& LRV) { print(LRV, std::cout); }
