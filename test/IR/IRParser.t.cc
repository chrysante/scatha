#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "IR/Type.h"

#include "test/IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("Parse simple ir-function", "[ir][parser]") {
    auto const text = R"(
func i64 @testfn(i64) {
  %entry:
    return i64 %0
})";
    auto [ctx, mod] = ir::parse(text).value();
    auto fnItr      = ranges::find_if(mod,
                                 [](auto& f) { return f.name() == "testfn"; });
    REQUIRE(fnItr != mod.end());
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

TEST_CASE("Parse IR with insert_value/extract_value", "[ir][parser]") {
    auto const text = R"(
struct @X {
  f64,
  i64
}
func @X @f(@X) {
  %entry:
    %1 = extract_value @X %0, 0
    %2 = extract_value @X %0, 1
    %res = insert_value @X %0, i64 7, 1
    return @X %res
})";
    auto [ctx, mod] = ir::parse(text).value();
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
