#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"

#include "test/IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("Parse simple ir-function", "[ir][parser]") {
    auto const text = R"(
function i64 @testfn(i64) {
  %entry:
    return i64 %0
})";
    ir::Context ctx;
    ir::Module mod = ir::parse(text, ctx).value();
    auto fnItr     = ranges::find_if(mod.functions(),
                                 [](auto& f) { return f.name() == "testfn"; });
    REQUIRE(fnItr != mod.functions().end());
    ir::Function& fn = *fnItr;
    CHECK(fn.name() == "testfn");
    CHECK(fn.returnType() == ctx.integralType(64));
    auto& firstParam = fn.parameters().front();
    CHECK(firstParam.type() == ctx.integralType(64));
    ir::BasicBlock& entry = fn.front();
    CHECK(entry.name() == "entry");
    ir::Return& ret     = cast<ir::Return&>(entry.front());
    ir::Value* retValue = ret.value();
    CHECK(retValue == &firstParam);
}

TEST_CASE("Parse typical front-end output", "[ir][parser]") {
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
}
function i64 @main() {
  %entry:
    %i-ptr = alloca i64
    %expr.result = neg i64 $3
    store %i-ptr, %expr.result
    %i = load i64 %i-ptr
    %expr.result.1 = neg i64 %i
    %call.result = call i64 @f, i64 %expr.result.1
    return i64 %call.result
})";
    ir::Context ctx;
    ir::Module mod = ir::parse(text, ctx).value();
    using namespace test::ir;
    using enum ir::NodeType;
    // clang-format off
    testModule(&mod).functions({
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
        }),
        testFunction("main").basicBlocks({
            testBasicBlock("entry").instructions({
                testInstruction("i-ptr")
                    .instType(Alloca),
                testInstruction("expr.result")
                    .instType(UnaryArithmeticInst),
                testInstruction("")
                    .instType(Store)
                    .references({ "i-ptr", "expr.result" }),
                testInstruction("i")
                    .instType(Load)
                    .references({ "i-ptr" }),
                testInstruction("expr.result.1")
                    .instType(UnaryArithmeticInst)
                    .references({ "i" }),
                testInstruction("call.result")
                    .instType(FunctionCall)
                    .references({ "expr.result.1" }),
                testInstruction("")
                    .instType(Return)
                    .references({ "call.result" }),
            })
        })
    });
    // clang-format on
}

TEST_CASE("Parse IR with insert_value/extract_value", "[ir][parser]") {
    auto const text = R"(
structure @X {
  f64,
  i64
}
function @X @f(@X) {
  %entry:
    %1 = extract_value @X %0, $0
    %2 = extract_value @X %0, $1
    %res = insert_value @X %0, i64 $7, $1
    return @X %res
})";
    ir::Context ctx;
    ir::Module mod = ir::parse(text, ctx).value();
    using namespace test::ir;
    using enum ir::NodeType;
    // clang-format off
    testModule(&mod).structures({
        testStructure("X").members({ "f64", "i64" })
    }).functions({
        testFunction("f").parameters({ "X" }).basicBlocks({
            testBasicBlock("entry").instructions({
                testInstruction("1")
                    .instType(ExtractValue)
                    .references({ "0" }),
                testInstruction("2")
                    .instType(ExtractValue)
                    .references({ "0" }),
                testInstruction("res")
                    .instType(InsertValue)
                    .references({ "0" }),
                testInstruction("")
                    .instType(InsertValue)
                .references({ "res" })
            })
        })
    });
    // clang-format on
}
