#include <Catch/Catch2.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "Opt/MemToReg.h"
#include "test/IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("MemToReg - 1", "[opt][mem-to-reg]") {
    auto const text              = R"(
func i64 @f(i64) {
  %entry:
    %i = alloca i64
    %result = alloca i64
    %j = alloca i64
    %tmp = alloca i64
    %tmp.4 = alloca i64
    store ptr %i, i64 %0
    store ptr %result, i64 0
    store ptr %j, i64 1
    goto label %loop.header

  %loop.header:
    %j.1 = load i64, ptr %j
    %cmp.result = cmp leq i64 %j.1, i64 5
    branch i1 %cmp.result, label %loop.body, label %loop.end

  %loop.body:
    %result.1 = load i64, ptr %result
    %expr.result = urem i64 %result.1, i64 2
    %cmp.result.1 = cmp eq i64 %expr.result, i64 0
    branch i1 %cmp.result.1, label %then, label %else

  %loop.end:
    %result.4 = load i64, ptr %result
    return i64 %result.4

  %then:
    %result.2 = load i64, ptr %result
    %j.2 = load i64, ptr %j
    %expr.result.1 = add i64 %result.2, i64 %j.2
    store ptr %tmp, i64 %expr.result.1
    %tmp.1 = load i64, ptr %tmp
    %tmp.2 = load i64, ptr %tmp
    store ptr %result, i64 %tmp.2
    %tmp.3 = load i64, ptr %result
    goto label %if.end

  %else:
    %result.3 = load i64, ptr %result
    %expr.result.2 = mul i64 2, i64 %result.3
    %j.3 = load i64, ptr %j
    %expr.result.3 = sub i64 %expr.result.2, i64 %j.3
    store ptr %tmp.4, i64 %expr.result.3
    %tmp.5 = load i64, ptr %tmp.4
    store ptr %result, i64 %tmp.5
    %tmp.6 = load i64, ptr %result
    goto label %if.end

  %if.end:
    %++.value.1 = load i64, ptr %j
    %++.result = add i64 %++.value.1, i64 1
    store ptr %j, i64 %++.result
    goto label %loop.header
})";
    auto [ctx, mod]              = ir::parse(text).value();
    auto& f                      = mod.functions().front();
    bool const modifiedFirstTime = opt::memToReg(ctx, f);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = opt::memToReg(ctx, f);
    CHECK(!modifiedSecondTime); /// `mem2reg` should be idempotent
    using namespace test::ir;
    using enum scatha::ir::NodeType;
    // clang-format off
    testModule(&mod).functions({
        testFunction("f").parameters({ "i64" }).basicBlocks({
            testBasicBlock("entry").instructions({
                testInstruction("")
                    .instType(Goto)
            }),
            testBasicBlock("loop.header").instructions({
                testInstruction("j.0")
                    .instType(Phi)
                    .references({ "++.result" }),
                testInstruction("result.0")
                    .instType(Phi)
                    .references({ "result.5" }),
                testInstruction("cmp.result")
                    .instType(CompareInst)
                    .references({ "j.0" }),
                testInstruction("")
                    .instType(Branch)
                    .references({ "cmp.result" })
            }),
            testBasicBlock("loop.body").instructions({
                testInstruction("expr.result")
                    .instType(ArithmeticInst)
                    .references({ "result.0" }),
                testInstruction("cmp.result.1")
                    .instType(CompareInst)
                    .references({ "expr.result" }),
                testInstruction("")
                    .instType(Branch)
                    .references({ "cmp.result.1" })
            }),
            testBasicBlock("loop.end").instructions({
                testInstruction("")
                    .instType(Return)
                    .references({ "result.0" })
            }),
            testBasicBlock("then").instructions({
                testInstruction("expr.result.1")
                    .instType(ArithmeticInst)
                    .references({ "result.0", "j.0" }),
                testInstruction("")
                    .instType(Goto)
            }),
            testBasicBlock("else").instructions({
                testInstruction("expr.result.2")
                    .instType(ArithmeticInst)
                    .references({ "result.0" }),
                testInstruction("expr.result.3")
                    .instType(ArithmeticInst)
                    .references({ "expr.result.2", "j.0" }),
                testInstruction("")
                    .instType(Goto)
            }),
            testBasicBlock("if.end").instructions({
                testInstruction("result.5")
                    .instType(Phi)
                    .references({ "expr.result.1", "expr.result.3" }),
                testInstruction("++.result")
                    .instType(ArithmeticInst)
                    .references({ "j.0" }),
                testInstruction("")
                    .instType(Goto)
            })
        })
    }); // clang-format on
}

TEST_CASE("MemToReg - 2", "[opt][mem-to-reg]") {
    auto const text              = R"(
func i64 @f(i64) {
  %entry:
    %n = alloca i64
    %i = alloca i64
    store ptr %i, i64 %0
    %i.1 = load i64, ptr %i
    %cmp.result = cmp grt i64 %i.1, i64 0
    branch i1 %cmp.result, label %then, label %if.end

  %then:
    store ptr %n, i64 1
    %tmp = load i64, ptr %n
    goto label %if.end

  %if.end:
    %n.1 = load i64, ptr %n
    return i64 %n.1
})";
    auto [ctx, mod]              = ir::parse(text).value();
    auto& f                      = mod.functions().front();
    bool const modifiedFirstTime = opt::memToReg(ctx, f);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = opt::memToReg(ctx, f);
    CHECK(!modifiedSecondTime); /// `mem2reg` should be idempotent
    using namespace test::ir;
    using enum scatha::ir::NodeType;
    // clang-format off
    testModule(&mod).functions({
        testFunction("f").parameters({ "i64" }).basicBlocks({
            testBasicBlock("entry").instructions({
                testInstruction("cmp.result")
                    .instType(CompareInst)
                    .references({ "0" }),
                testInstruction("")
                    .instType(Branch)
                    .references({ "cmp.result" }),
            }),
            testBasicBlock("then").instructions({
                testInstruction("")
                    .instType(Goto)
            }),
            testBasicBlock("if.end").instructions({
                testInstruction("n.0")
                    .instType(Phi),
                testInstruction("")
                    .instType(Return)
                    .references({ "n.0" })
            }),
        })
    }); // clang-format on
}
