#include "IRGen/SyntheticFunction.h"

#include "IR/CFG.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

void irgen::generateSyntheticFunction(sema::Function const* semaFn,
                                      ir::Function* irFn,
                                      ir::Context& ctx) {
    auto* BB = new ir::BasicBlock(ctx, "entry");
    BB->pushBack(new ir::Return(ctx));
    irFn->pushBack(BB);
}
