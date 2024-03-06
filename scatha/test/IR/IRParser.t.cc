#include <catch2/catch_test_macros.hpp>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/Attributes.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"

#include "IR/EqualityTestHelper.h"

using namespace scatha;

TEST_CASE("Parse simple ir-function", "[ir][parser]") {
    auto const text = R"(
func i64 @testfn(i64) {
  %entry:
    return i64 %0
})";
    auto [ctx, mod] = ir::parse(text).value();
    auto fnItr =
        ranges::find_if(mod, [](auto& f) { return f.name() == "testfn"; });
    REQUIRE(fnItr != mod.end());
    ir::Function& fn = *fnItr;
    CHECK(fn.name() == "testfn");
    CHECK(fn.returnType() == ctx.intType(64));
    auto& firstParam = fn.parameters().front();
    CHECK(firstParam.type() == ctx.intType(64));
    ir::BasicBlock& entry = fn.front();
    CHECK(entry.name() == "entry");
    ir::Return& ret = cast<ir::Return&>(entry.front());
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

TEST_CASE("Parse IR with pointer info metadata", "[ir][parser]") {
    auto const text = R"(
func void @f() {
%entry:
    %1 = alloca i64, i32 1 #ptr(align: 8, validsize: 8, provenance: ptr %1, offset: 0, nonnull)
    // Same as %1 but metadata in different order
    %2 = alloca i64, i32 1 #ptr(nonnull, validsize: 8, provenance: ptr %1, align: 8, offset: 0)
    return
})";
    auto [ctx, mod] = ir::parse(text).value();
    auto& f = mod.front();
    auto& entry = f.front();
    auto& a1 = entry.front();
    SECTION("%1") {
        auto* ptr = a1.pointerInfo();
        REQUIRE(ptr);
        CHECK(ptr->align() == 8);
        CHECK(ptr->validSize().value() == 8);
        CHECK(ptr->provenance() == &a1);
        CHECK(ptr->staticProvencanceOffset().value() == 0);
        CHECK(ptr->guaranteedNotNull());
    }
    auto& a2 = *a1.next();
    SECTION("%1") {
        auto* ptr = a2.pointerInfo();
        REQUIRE(ptr);
        CHECK(ptr->align() == 8);
        CHECK(ptr->validSize().value() == 8);
        CHECK(ptr->provenance() == &a1);
        CHECK(ptr->staticProvencanceOffset().value() == 0);
        CHECK(ptr->guaranteedNotNull());
    }
}

TEST_CASE("Parse parameters with valret and byval atribute", "[ir][parser]") {
    auto const text = R"(
func void @f(ptr valret(size: 24, align: 4) %0,
             ptr byval(align: 8, size: 32) %1) {
%entry:
    return
})";
    auto [ctx, mod] = ir::parse(text).value();
    auto& f = mod.front();
    {
        auto* ret = &f.parameters().front();
        auto* attrib = ret->get<ir::ValRetAttribute>();
        REQUIRE(attrib);
        CHECK(attrib->size() == 24);
        CHECK(attrib->align() == 4);
    }
    {
        auto* arg = &f.parameters().back();
        auto* attrib = arg->get<ir::ByValAttribute>();
        REQUIRE(attrib);
        CHECK(attrib->size() == 32);
        CHECK(attrib->align() == 8);
    }
}
