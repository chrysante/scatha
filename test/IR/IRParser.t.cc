#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"

using namespace scatha;

TEST_CASE("Parse simple ir-function", "[ir][parser]") {
    auto const text = R"(
function i64 @testfn(i64) {
  %entry:
    return i64 %0
})";
    ir::Context ctx;
    ir::Module mod = ir::parse(text, ctx).value();
    auto fnItr = ranges::find_if(mod.functions(), [](auto& f) { return f.name() == "testfn"; });
    REQUIRE(fnItr != mod.functions().end());
    ir::Function& fn = *fnItr;
    CHECK(fn.name() == "testfn");
    CHECK(fn.returnType() == ctx.integralType(64));
    auto& firstParam = fn.parameters().front();
    CHECK(firstParam.type() == ctx.integralType(64));
    ir::BasicBlock& entry = fn.front();
    CHECK(entry.name() == "entry");
    ir::Return& ret = cast<ir::Return&>(entry.front());
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
    /// Check `f`
    auto fItr = ranges::find_if(mod.functions(), [](auto& f) { return f.name() == "f"; });
    REQUIRE(fItr != mod.functions().end());
    ir::Function& f = *fItr;
    CHECK(f.name() == "f");
    CHECK(f.returnType() == ctx.integralType(64));
    auto& firstParam = f.parameters().front();
    CHECK(firstParam.type() == ctx.integralType(64));
    std::array fBBNames = {
        "entry", "if.then", "if.end"
    };
    for (auto&& [index, bb]: f | ranges::views::enumerate) {
        CHECK(bb.name() == fBBNames[index]);
        
    }
    std::array const fInstNames = {
        "i.addr", "", "j.ptr", "i", "expr.result", "", "i.1", "cmp.result",
        "", "++.value.1", "++.result", "", "", "j", ""
    };
    using enum ir::NodeType;
    std::array const fInstTypes = {
        Alloca, Store, Alloca, Load, ArithmeticInst, Store, Load, CompareInst,
        Branch, Load, ArithmeticInst, Store, Goto, Load, Return
    };
    for (auto&& [index, inst]: f.instructions() | ranges::views::enumerate) {
        CHECK(inst.name() == fInstNames[index]);
        CHECK(inst.nodeType() == fInstTypes[index]);
    }
}

TEST_CASE("Parse IR with insert_value/extract_value", "[ir][parser]") {
    auto const text = R"(
structure @X {
  i64,
  f64
}
function @X @f(@X) {
  %entry:
    %1 = extract_value @X %0, i64 $0
    %2 = extract_value @X %0, i64 $1
    %res = insert_value @X %0, i64 $7, i64 $1
    return @X %res
})";
    ir::Context ctx;
    ir::Module mod = ir::parse(text, ctx).value();
    /// Check `X`
    auto* X = mod.findStructure("X");
    CHECK(X != nullptr);
    CHECK(X->name() == "X");
    CHECK(X->members().size() == 2);
    CHECK(X->memberAt(0)->name() == "i64");
    CHECK(X->memberAt(1)->name() == "f64");
}
