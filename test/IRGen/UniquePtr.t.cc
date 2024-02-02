#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Equal.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("Unique expr of int", "[irgen]") {
    using namespace ir;
    std::string source = "public fn foo() -> int { return *(unique int(42)); }";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto* eight = ctx.intConstant(8, 64);

    auto entry = BBView(F.entry());
    CHECK_NOTHROW(entry.nextAs<Alloca>());
    auto& alloc = entry.nextAs<Call>();
    CHECK(alloc.function()->name() == "__builtin_alloc");
    CHECK(alloc.argumentAt(0) == eight);
    CHECK(alloc.argumentAt(1) == eight);
    auto& data = entry.nextAs<ExtractValue>();
    CHECK(data.baseValue() == &alloc);
    CHECK_NOTHROW(entry.nextAs<Store>());
    auto& store = entry.nextAs<Store>();
    CHECK(store.address() == &data);
    CHECK(store.value() == ctx.intConstant(42, 64));
    CHECK(entry.nextAs<Load>().type() == ctx.ptrType());
    CHECK(entry.nextAs<Load>().type() == ctx.intType(64));
    CHECK(entry.nextAs<Load>().type() == ctx.ptrType());
    CHECK_NOTHROW(entry.nextAs<CompareInst>());
    CHECK_NOTHROW(entry.nextAs<Branch>());

    auto del = entry.nextBlock();
    auto& dealloc = del.nextAs<Call>();
    CHECK(dealloc.function()->name() == "__builtin_dealloc");
    CHECK(dealloc.argumentAt(1) == eight);
    CHECK(dealloc.argumentAt(2) == eight);

    auto end = del.nextBlock();
    CHECK_NOTHROW(end.nextAs<Return>());
}

TEST_CASE("Unique expr of array with nontrivial def ctor", "[irgen]") {
    using namespace ir;
    std::string source = R"(
struct X {
    fn new(&mut this) {}
}
public fn foo() {
    unique [X](10);
}
)";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto entry = BBView(F.entry());
    CHECK(entry.nextIs<Alloca>());
    auto& alloc = entry.nextAs<Call>();
    CHECK(alloc.function()->name() == "__builtin_alloc");
    auto& gotoBody = entry.terminatorAs<Goto>();
    auto body = entry.nextBlock();
    CHECK(gotoBody.target() == body.BB);
    CHECK(body.terminatorIs<Branch>());
    auto constrEnd = body.nextBlock();
    auto del = constrEnd.nextBlock();
    CHECK(del.terminatorIs<Goto>());
    auto delEnd = del.nextBlock();
    CHECK(delEnd.nextIs<Return>());
}

TEST_CASE("Destruction of unique pointer to array function argument",
          "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public struct Bar {
    fn delete(&mut this) {}
}
public fn foo(p: *unique [Bar]) {}
)" });
    auto& foo = mod.back();
    auto& bar_delete = mod.front();

    auto entry = BBView(foo.entry());
    CHECK(entry.terminatorIs<Branch>());

    auto deleteBlock = entry.nextBlock();
    CHECK(deleteBlock.terminatorIs<Goto>());

    auto arrayLoopBody = deleteBlock.nextBlock();
    CHECK(ranges::count_if(*arrayLoopBody.BB | Filter<Call>,
                           [&](Call const& call) {
        return call.function() == &bar_delete;
    }) == 1);
    CHECK(arrayLoopBody.terminatorIs<Branch>());

    auto arrayLoopEnd = arrayLoopBody.nextBlock();
    CHECK(arrayLoopEnd.terminatorIs<Goto>());

    auto deleteEnd = arrayLoopEnd.nextBlock();
    CHECK(deleteEnd.terminatorIs<Return>());
}

TEST_CASE("Return unique pointer", "[irgen]") {
    using namespace ir;
    std::string source = R"(
fn bar() -> *unique int { return unique int(0); }
public fn foo() -> *unique int { return bar(); }
)";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto entry = BBView(F.entry());

    CHECK(entry.nextIs<Alloca>());
    CHECK(entry.nextIs<Call>());
    CHECK(entry.nextIs<Store>());
    CHECK(entry.nextIs<Load>());
    CHECK(entry.nextIs<Return>());
}

TEST_CASE("Implicit two step conversion", "[irgen]") {
    using namespace ir;
    std::string source = R"(
public fn foo(p: &*unique [int, 2]) { bar(p); }
fn bar(p: *[int]) {}
)";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    auto entry = BBView(F.entry());

    auto& load = entry.nextAs<Load>();
    CHECK(load.address() == &F.parameters().front());
    auto& call = entry.nextAs<Call>();
    CHECK(call.function() == &mod.back());
    CHECK(call.argumentAt(0) == &load);
    CHECK(call.argumentAt(1) == ctx.intConstant(2, 64));
    CHECK(entry.nextIs<Return>());
}
