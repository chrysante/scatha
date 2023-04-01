#include <Catch/Catch2.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/InlineCallsite.h"
#include "Opt/InstCombine.h"
#include "Opt/MemToReg.h"
#include "Opt/SROA.h"
#include "Opt/SimplifyCFG.h"
#include "test/IR/EqualityTestHelper.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("SROA - 1", "[opt][mem-to-reg]") {
    auto const text = R"(
struct @X { i64, i64 }
func i64 @main() {
  %entry:
    %a = alloca @X, i32 10
    %index.0 = add i32 1, i32 2
    %p.0 = call ptr @populate, ptr %a, i32 %index.0
    %q.0 = getelementptr inbounds @X, ptr %p.0, i32 0, 1
    %p.1 = call ptr @populate, ptr %a, i32 5
    %q.1 = getelementptr inbounds @X, ptr %p.1, i32 0, 0
    %res.0 = load i64, ptr %q.0
    %res.1 = load i64, ptr %q.1
    %res = add i64 %res.0, i64 %res.1
    return i64 %res
}
func ptr @populate(ptr %a, i32 %index) {
  %entry:
    %p = getelementptr inbounds @X, ptr %a, i32 %index
    %x.0 = insert_value @X undef, i64 1, 0
    %x.1 = insert_value @X %x.0, i64 2, 1
    store ptr %p, @X %x.1
    return ptr %p
})";
    auto [ctx, mod] = ir::parse(text).value();
    auto& main      = mod.front();
    auto itr        = main.instructions().begin();
    ++ ++itr;
    auto* call1 = cast<Call*>(std::to_address(itr));
    ++ ++itr;
    auto* call2 = cast<Call*>(std::to_address(itr));
    inlineCallsite(ctx, call1);
    inlineCallsite(ctx, call2);
    propagateConstants(ctx, main);
    for (int i = 0; i < 2; ++i) {
        sroa(ctx, main);
        memToReg(ctx, main);
        instCombine(ctx, main);
        dce(ctx, main);
        simplifyCFG(ctx, main);
        propagateConstants(ctx, main);
    }
    using namespace test::ir;
    using enum scatha::ir::NodeType;
    testFunction("main")
        .basicBlocks({
            testBasicBlock("entry").instructions({
                testInstruction("").instType(Return).references({ "" }),
            }),
        })
        .test(main);
}
