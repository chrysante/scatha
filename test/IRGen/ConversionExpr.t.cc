#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("Array static to dynamic conversion", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(a: &[int, 3]) -> &[int]  { return a; }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& insertData = view.nextAs<InsertValue>();
    CHECK(insertData.insertedValue() == &F.parameters().front());
    auto& insertCount = view.nextAs<InsertValue>();
    CHECK(insertCount.insertedValue() == ctx.intConstant(3, 64));
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &insertCount);
}

TEST_CASE("Array pointer static to dynamic conversion", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(a: *[int, 3]) -> *[int]  { return a; }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK_NOTHROW(view.nextAs<Alloca>());
    CHECK_NOTHROW(view.nextAs<Store>());
    auto& loadPtr = view.nextAs<Load>();
    auto& insertData = view.nextAs<InsertValue>();
    CHECK(insertData.insertedValue() == &loadPtr);
    auto& insertCount = view.nextAs<InsertValue>();
    CHECK(insertCount.insertedValue() == ctx.intConstant(3, 64));
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &insertCount);
}
