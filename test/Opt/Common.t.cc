#include <Catch/Catch2.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "Opt/Common.h"
#include "test/IR/CompileToIR.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("Phi compareEqual()", "[opt]") {
    Context ctx;
    Function f(nullptr, ctx.voidType(), {}, "f");
    BasicBlock bb1(ctx, "bb1");
    bb1.set_parent(&f);
    BasicBlock bb2(ctx, "bb2");
    bb2.set_parent(&f);
    BasicBlock bb3(ctx, "bb3");
    bb3.set_parent(&f);
    Value* const one   = ctx.integralConstant(1, 64);
    Value* const two   = ctx.integralConstant(2, 64);
    Value* const three = ctx.integralConstant(3, 64);
    Phi p1({ { &bb1, one }, { &bb2, two } }, "phi1");
    p1.set_parent(&bb3);
    Phi p2({ { &bb1, one }, { &bb2, three } }, "phi2");
    p2.set_parent(&bb3);
    CHECK(!compareEqual(&p1, &p2));
}
