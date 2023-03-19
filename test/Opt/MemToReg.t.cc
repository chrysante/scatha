#include <Catch/Catch2.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "Opt/MemToReg.h"
#include "test/IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("MemToReg - 1", "[opt][mem-to-reg]") {
    auto const text = R"(
function i64 @f(i64) {
  %entry:
    %i = alloca i64
    %result = alloca i64
    %j = alloca i64
    %tmp = alloca i64
    %tmp.4 = alloca i64
    store %i, %0
    store %result, $0
    store %j, $1
    goto label %loop.header
  %loop.header:
    %j.1 = load i64 %j
    %cmp.result = cmp leq i64 %j.1, i64 $5
    branch i1 %cmp.result, label %loop.body, label %loop.end
  %loop.body:
    %result.1 = load i64 %result
    %expr.result = rem i64 %result.1, $2
    %cmp.result.1 = cmp eq i64 %expr.result, i64 $0
    branch i1 %cmp.result.1, label %then, label %else
  %loop.end:
    %result.4 = load i64 %result
    return i64 %result.4
  %then:
    %result.2 = load i64 %result
    %j.2 = load i64 %j
    %expr.result.1 = add i64 %result.2, %j.2
    store %tmp, %expr.result.1
    %tmp.1 = load i64 %tmp
    %tmp.2 = load i64 %tmp
    store %result, %tmp.2
    %tmp.3 = load i64 %result
    goto label %if.end
  %else:
    %result.3 = load i64 %result
    %expr.result.2 = mul i64 $2, %result.3
    %j.3 = load i64 %j
    %expr.result.3 = sub i64 %expr.result.2, %j.3
    store %tmp.4, %expr.result.3
    %tmp.5 = load i64 %tmp.4
    store %result, %tmp.5
    %tmp.6 = load i64 %result
    goto label %if.end
  %if.end:
    %++.value.1 = load i64 %j
    %++.result = add i64 %++.value.1, $1
    store %j, %++.result
    goto label %loop.header
})";
    ir::Context ctx;
    ir::Module mod               = ir::parse(text, ctx).value();
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
                testInstruction("j.1")
                    .instType(Phi)
                    .references({ "++.result" }),
                testInstruction("result.1")
                    .instType(Phi)
                    .references({ "result.3" }),
                testInstruction("cmp.result")
                    .instType(CompareInst)
                    .references({ "j.1" }),
                testInstruction("")
                    .instType(Branch)
                    .references({ "cmp.result" })
            }),
            testBasicBlock("loop.body").instructions({
                testInstruction("expr.result")
                    .instType(ArithmeticInst)
                    .references({ "result.1" }),
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
                    .references({ "result.1" })
            }),
            testBasicBlock("then").instructions({
                testInstruction("expr.result.1")
                    .instType(ArithmeticInst)
                    .references({ "result.1", "j.1" }),
                testInstruction("")
                    .instType(Goto)
            }),
            testBasicBlock("else").instructions({
                testInstruction("expr.result.2")
                    .instType(ArithmeticInst)
                    .references({ "result.1" }),
                testInstruction("expr.result.3")
                    .instType(ArithmeticInst)
                    .references({ "expr.result.2", "j.1" }),
                testInstruction("")
                    .instType(Goto)
            }),
            testBasicBlock("if.end").instructions({
                testInstruction("result.3")
                    .instType(Phi)
                    .references({ "expr.result.1", "expr.result.3" }),
                testInstruction("++.result")
                    .instType(ArithmeticInst)
                    .references({ "j.1" }),
                testInstruction("")
                    .instType(Goto)
            })
        })
    }); // clang-format on
}

TEST_CASE("MemToReg - 2", "[opt][mem-to-reg]") {
    auto const text = R"(
function i64 @f(i64) {
  %entry:
    %n = alloca i64
    %i = alloca i64
    store %i, %0
    %i.1 = load i64 %i
    %cmp.result = cmp grt i64 %i.1, i64 $0
    branch i1 %cmp.result, label %then, label %if.end
  %then:
    store %n, $1
    %tmp = load i64 %n
    goto label %if.end
  %if.end:
    %n.1 = load i64 %n
    return i64 %n.1
})";
    ir::Context ctx;
    ir::Module mod               = ir::parse(text, ctx).value();
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
                testInstruction("n.1")
                    .instType(Phi),
                testInstruction("")
                    .instType(Return)
                    .references({ "n.1" })
            }),
        })
    }); // clang-format on
}
