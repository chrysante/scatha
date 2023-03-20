#include "Volatile.h"

#include <iostream>
#include <range/v3/view.hpp>

#include "Basic/Basic.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "IRDump.h"
#include "Opt/CallGraph.h"

using namespace scatha;

void playground::volatilePlayground(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    auto callGraph = opt::CallGraph::build(mod);
    auto sccs      = opt::computeSCCs(callGraph);
    for (auto& scc: sccs) {
        auto names = scc | ranges::views::transform(
                               [](ir::Function const* f) { return f->name(); });
        std::cout << names << std::endl;
    }
}
