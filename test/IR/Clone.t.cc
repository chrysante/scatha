#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"

#include "test/IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("IR clone", "[ir][parser]") {
    auto const text = R"(
func i64 @f(i64) {
  %entry:
    %i.addr = alloca i64
    store ptr %i.addr, i64 %0
    %j.ptr = alloca i64
    %i = load i64, ptr %i.addr
    %expr.result = mul i64 %i, i64 2
    store ptr %j.ptr, i64 %expr.result
    %i.1 = load i64, ptr %i.addr
    %cmp.result = scmp grt i64 %i.1, i64 2
    branch i1 %cmp.result, label %if.then, label %if.end

  %if.then:
    %++.value.1 = load i64, ptr %j.ptr
    %++.result = add i64 %++.value.1, i64 1
    store ptr %j.ptr, i64 %++.result
    goto label %if.end

  %if.end:
    %j = load i64, ptr %j.ptr
    return i64 %j
})";
    auto [ctx, mod] = ir::parse(text).value();
    auto& f         = mod.front();
    auto fClone     = ir::clone(ctx, &f);
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
