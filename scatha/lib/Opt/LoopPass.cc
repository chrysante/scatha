#include "Opt/Passes.h"

#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "IR/CFG/Function.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;
using namespace ranges::views;

SC_REGISTER_FUNCTION_PASS(
    [](Context& ctx, Function& F, LoopPass const& loopPass,
       PassArgumentMap const&) { return loopSchedule(ctx, F, loopPass); },
    "loop", PassCategory::Schedule, {});

#include "IR/Print.h"
#include <iostream>
static bool loopPrepare(Context&, LNFNode& loop) {
    std::cout << format(*loop.basicBlock()) << std::endl;
    return false;
}

bool opt::loopSchedule(Context& ctx, Function& F, LoopPass const& loopPass) {
    auto& LNF = F.getOrComputeLNF();
    auto queue = LNF.roots() | ToSmallVector<>;
    for (size_t i = 0; i < queue.size(); ++i) {
        auto* node = queue[i];
        auto children = node->children();
        queue.insert(queue.end(), children.begin(), children.end());
    }
    bool modified = false;
    for (auto* node: queue | reverse) {
        if (!node->isProperLoop()) {
            continue;
        }
        modified |= loopPrepare(ctx, *node);
        modified |= loopPass(ctx, *node);
    }
    return modified;
}
