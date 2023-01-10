#include "Pathfinder.h"

#include <map>
#include <span>

#include "Opt/ControlFlowPath.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct Ctx {
    using Map = std::map<size_t, ControlFlowPath>;
    using MapIter = Map::iterator;
    
    explicit Ctx(Instruction const* origin, Instruction const* dest):
        origin(origin),
        dest(dest),
        originBB(origin->parent()),
        destBB(dest->parent()) {}
    
    void run() {
        auto [itr, success] = result.insert({ id++, ControlFlowPath(origin, dest)});
        SC_ASSERT(success, "Why not?");
        search(itr, originBB);
    }
    
    static bool containsTwice(std::span<BasicBlock const* const> path, BasicBlock const* bb) {
        int count = 0;
        for (auto& pathBB: path) {
            count += pathBB == bb;
            if (count == 2) { return true; }
        }
        return false;
    }
    
    void search(MapIter currentPathItr, BasicBlock const* currentNode) {
        auto& currentPath = currentPathItr->second;
        /// We allow nodes occuring twice to allow cycles, but we only want to traverse the cycle once.
        if (containsTwice(currentPath.basicBlocks(), currentNode)) {
            result.erase(currentPathItr);
            return;
        }
        opt::internal::addBasicBlock(currentPath, currentNode);
        if (currentNode == destBB && currentPath.basicBlocks().size() > 1) {
            return;
        }
        switch (currentNode->successors().size()) {
        case 0:
            result.erase(currentPathItr);
            break;
        case 1:
            search(currentPathItr, currentNode->successors().front());
            break;
        default:
            auto cpCopy = currentPath;
            search(currentPathItr, currentNode->successors().front());
            for (size_t i = 1; i < currentNode->successors().size(); ++i) {
                auto [itr, success] = result.insert({ id++, cpCopy });
                SC_ASSERT(success, "");
                search(itr, currentNode->successors()[i]);
            }
            break;
        }
    }
    
    Instruction const* origin;
    Instruction const* dest;
    BasicBlock const* originBB;
    BasicBlock const* destBB;
    size_t id = 0;
    Map result{};
};

} // namespace

utl::vector<ControlFlowPath> opt::findAllPaths(ir::Instruction const* origin, ir::Instruction const* dest) {
    Ctx ctx{ origin, dest };
    ctx.run();
    return utl::vector<ControlFlowPath>(
              utl::transform(std::move(ctx.result),
              [](auto&& pair) -> decltype(auto) { return UTL_FORWARD(pair).second; }));
}
