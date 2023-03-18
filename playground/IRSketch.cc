#include "IRSketch.h"

#include <iostream>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "Opt/Dominance.h"

using namespace scatha;

void playground::irSketch() {
    ir::Context ctx;
    auto const text = R"(
function i64 @f() {
  %entry:
    goto label %2
  %2:
    %cond = cmp leq i64 $1, i64 $2
    branch i1 %cond, label %3, label %4
  %3:
    goto label %5
  %4:
    goto label %5
  %5:
    goto label %6
  %6:
    goto label %7
  %7:
    branch i1 %cond, label %8, label %6
  %8:
    return i64 $0
})";
    ir::Module mod  = ir::parse(text, ctx).value();

    auto& f = mod.functions().front();
    
    auto domTree = opt::buildDomTree(f);

    opt::print(domTree);
    
    auto domFronts = opt::computeDominanceFrontiers(f, domTree);
    
    for (auto&& [bb, df]: domFronts) {
        auto dfNames = df |
                       ranges::views::transform([](ir::BasicBlock const* bb) {
                           return bb->name();
                       });
        std::cout << bb->name() << ": " << dfNames << std::endl;
    }
}
