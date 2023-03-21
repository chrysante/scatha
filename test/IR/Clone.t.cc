#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"

#include "test/IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("IR clone", "[ir][parser]") {
    auto const text = R"(
function i64 @f(i64) {
  %entry:
    %i.addr = alloca i64
    store %i.addr, %0
    %j.ptr = alloca i64
    %i = load i64 %i.addr
    %expr.result = mul i64 %i, $2
    store %j.ptr, %expr.result
    %i.1 = load i64 %i.addr
    %cmp.result = cmp grt i64 %i.1, i64 $2
    branch i1 %cmp.result, label %if.then, label %if.end
  %if.then:
    %++.value.1 = load i64 %j.ptr
    %++.result = add i64 %++.value.1, $1
    store %j.ptr, %++.result
    goto label %if.end
  %if.end:
    %j = load i64 %j.ptr
    return i64 %j
})";
    ir::Context ctx;
    ir::Module mod = ir::parse(text, ctx).value();
    auto& f        = mod.functions().front();
    auto fClone    = ir::clone(ctx, &f);
    using namespace test::ir;
    using enum ir::NodeType;
    // clang-format off
    testFunction("f").parameters({ "i64" }).basicBlocks({
        testBasicBlock("entry").instructions({
            testInstruction("i.addr")
                .instType(Alloca),
            testInstruction("")
                .instType(Store)
                .references({ "i.addr", "0" }),
            testInstruction("j.ptr")
                .instType(Alloca),
            testInstruction("i")
                .instType(Load)
                .references({ "i.addr" }),
            testInstruction("expr.result")
                .instType(ArithmeticInst)
                .references({ "i" }),
            testInstruction("")
                .instType(Store)
                .references({ "j.ptr", "expr.result" }),
            testInstruction("i.1")
                .instType(Load)
                .references({ "i.addr" }),
            testInstruction("cmp.result")
                .instType(CompareInst)
                .references({ "i.1" }),
            testInstruction("")
                .instType(Branch)
                .references({ "cmp.result" }),
        }),
        testBasicBlock("if.then").instructions({
            testInstruction("++.value.1")
                .instType(Load)
                .references({ "j.ptr" }),
            testInstruction("++.result")
                .instType(ArithmeticInst)
                .references({ "++.value.1" }),
            testInstruction("")
                .instType(Store)
                .references({ "j.ptr", "++.result" }),
            testInstruction("")
                .instType(Goto)
        }),
        testBasicBlock("if.end").instructions({
            testInstruction("j")
                .instType(Load)
                .references({ "j.ptr" }),
            testInstruction("")
                .instType(Return)
                .references({ "j" })
        })
    }).test(*fClone); // clang-format on
}
