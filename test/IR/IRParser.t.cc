#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"

using namespace scatha;
using namespace ir;

TEST_CASE("ir parser ...") {
    auto const text = R"(
function i64 @testfn(i64) {
  %entry:
    return i64 %0
})";
    ir::Context ctx;
    Module mod = parse(text, ctx).value();
    auto fnItr = ranges::find_if(mod.functions(), [](Function const& f) { return f.name() == "testfn"; });
    REQUIRE(fnItr != mod.functions().end());
    Function& fn = *fnItr;
    CHECK(fn.name() == "testfn");
    CHECK(fn.returnType() == ctx.integralType(64));
    auto& firstParam = fn.parameters().front();
    CHECK(firstParam.type() == ctx.integralType(64));
    BasicBlock& entry = fn.front();
    CHECK(entry.name() == "entry");
    Return& ret = cast<Return&>(entry.front());
    Value* retValue = ret.value();
    CHECK(retValue == &firstParam);
}
