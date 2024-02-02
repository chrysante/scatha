#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include <range/v3/algorithm.hpp>

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

TEST_CASE("Reinterpret pointer", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: *int) -> *double { return reinterpret<*double>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Alloca>());
    CHECK(view.nextIs<Store>());
    CHECK(view.nextIs<Load>());
    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret static array pointer as dyn byte array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: *[int, 2]) -> *[byte] { return reinterpret<*[byte]>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Alloca>());
    CHECK(view.nextIs<Store>());
    CHECK(view.nextIs<Load>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret dyn byte array pointer as int array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: *[byte]) -> *[int] { return reinterpret<*[int]>(p); }
)" });
    auto& F = mod.front();
    CHECK(ranges::any_of(F.entry(), isa<ArithmeticInst>));
}

TEST_CASE("Reinterpret dyn byte array pointer as int", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: *[byte]) -> *int { return reinterpret<*int>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Alloca>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<Store>());
    CHECK(view.nextIs<Load>());
    CHECK(view.nextIs<ExtractValue>());
    CHECK(view.terminatorIs<Return>());
}

TEST_CASE("Reinterpret int pointer as byte array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: *int) -> *[byte] { return reinterpret<*[byte]>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Alloca>());
    CHECK(view.nextIs<Store>());
    CHECK(view.nextIs<Load>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret reference", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: &int) -> &double { return reinterpret<&double>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret reference to static array as dyn byte array",
          "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: &[int, 2]) -> &[byte] { return reinterpret<&[byte]>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret reference to dyn array as dyn byte array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: &[byte]) -> &[int] { return reinterpret<&[int]>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<ArithmeticInst>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret dynamic byte array as int", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: &[byte]) -> &int { return reinterpret<&int>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret reference to int as dyn byte array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(p: &int) -> &[byte] { return reinterpret<&[byte]>(p); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<InsertValue>());
    CHECK(view.nextAs<InsertValue>().insertedValue() == ctx.intConstant(8, 64));
    CHECK(view.nextIs<Return>());
}

TEST_CASE("Reinterpret value", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(i: &int) -> double { return reinterpret<double>(i); }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK(view.nextIs<Load>());
    CHECK(view.nextAs<ConversionInst>().conversion() == Conversion::Bitcast);
    CHECK(view.nextIs<Return>());
}
