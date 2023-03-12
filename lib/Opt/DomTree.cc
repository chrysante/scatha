#include "Opt/DomTree.h"

#include <ostream>

#include <utl/hashtable.hpp>

#include "IR/CFG.h"

using namespace scatha;
using namespace opt;
using namespace ir;

void opt::print(DomTree const& domTree) {}

void opt::print(DomTree const& domTree, std::ostream& ostream) {}

DomTree::DomTree() = default;

DomTree opt::buildDomTree(ir::Function& function) {
    DomTree result;
    utl::hashmap<BasicBlock*, size_t> indexMap;
    for (auto& basicBlock: function.basicBlocks()) {
        indexMap.insert({ &basicBlock, result._nodes.size() });
        result._nodes.push_back({ &basicBlock });
    }

#if 0
    // dominator of the start node is the start itself
    Dom(n0) = {n0}
    // for all other nodes, set all nodes as the dominators
    for each n in N - {n0}
        Dom(n) = N;
    // iteratively eliminate nodes that are not dominators
    while changes in any Dom(n)
        for each n in N - {n0}:
            Dom(n) = {n} union with intersection over Dom(p) for all p in pred(n)
#endif // 0
    SC_UNREACHABLE();
}
