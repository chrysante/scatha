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

TEST_CASE("IRGen - Function call with reference to dynamic array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(data: &[int]) { bar(data); }
fn bar(data: &[int]) {}
)" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());

    auto& call = view.nextAs<Call>();
    CHECK(call.function() == F.next());
    CHECK(call.argumentAt(0) == &F.parameters().front());
    CHECK(call.argumentAt(1) == &F.parameters().back());
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Unpacking return value", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(data: &[int]) { bar(*baz()); }
fn bar(data: &[int]) {}
fn baz() -> *[int] {}
)" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());

    auto& callBaz = view.nextAs<Call>();
    CHECK(callBaz.arguments().empty());
    auto& data = view.nextAs<ExtractValue>();
    CHECK(data.baseValue() == &callBaz);
    auto& count = view.nextAs<ExtractValue>();
    CHECK(count.baseValue() == &callBaz);
    auto& callBar = view.nextAs<Call>();
    CHECK(callBar.argumentAt(0) == &data);
    CHECK(callBar.argumentAt(1) == &count);
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Return value passed on stack", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() { bar(); }
fn bar() -> [int, 10] {}
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    auto& call = view.nextAs<Call>();
    CHECK(call.argumentAt(0) == &mem);
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Pass return value in memory to return statement",
          "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() -> [int, 10] { return bar(); }
fn bar() -> [int, 10] {}
)" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    CHECK(isa<VoidType>(F.returnType()));
    auto view = BBView(F.entry());

    auto& call = view.nextAs<Call>();
    CHECK(call.argumentAt(0) == &F.parameters().front());
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Return .count of static array return value", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() -> int { return bar().count; }
fn bar() -> [int, 10] {}
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    CHECK_NOTHROW(view.nextAs<Alloca>());
    CHECK_NOTHROW(view.nextAs<Call>());
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == ctx.intConstant(10, 64));
}

TEST_CASE("IRGen - Return .count of dynamic array return value", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() -> int { return bar().count; }
fn bar() -> &[int] {}
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& call = view.nextAs<Call>();
    auto& count = view.nextAs<ExtractValue>();
    CHECK(count.baseValue() == &call);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &count);
}

TEST_CASE("IRGen - Function call with big object by value", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() { bar([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]); }
fn bar(arr: [int, 10]) {}
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == ctx.intType(64));
    CHECK(mem.count() == ctx.intConstant(10, 32));
    auto& memcpy = view.nextAs<Call>();
    CHECK(memcpy.function()->name() == "__builtin_memcpy");
    CHECK(memcpy.argumentAt(0) == &mem);
    CHECK(memcpy.argumentAt(1) == ctx.intConstant(80, 64));
    auto& call = view.nextAs<Call>();
    CHECK(call.argumentAt(0) == &mem);
}

TEST_CASE("IRGen - Function call returns reference", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() -> int { return get(); }
fn get() -> &int {}
)" });
    auto& foo = mod.front();
    auto& get = mod.back();
    auto view = BBView(foo.entry());

    auto& call = view.nextAs<Call>();
    CHECK(call.function() == &get);
    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &call);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &load);
}
