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

TEST_CASE("Member access simple", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public struct X { var i: int; }
public fn foo(x: &X) -> int { return x.i; }
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& addr = view.nextAs<GetElementPointer>();
    CHECK(addr.basePointer() == &F.parameters().front());
    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &addr);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &load);
}

TEST_CASE("Member access dynamic array pointer member", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public struct X { var ptr: *[int]; }
public fn foo(x: &X) -> int { return x.ptr[10]; }
)" });
    auto& F = mod.front();

    auto& ret = dyncast<Return const&>(*F.entry().terminator());
    auto& elemLoad = dyncast<Load const&>(*ret.value());
    auto& elemGep = dyncast<GetElementPointer const&>(*elemLoad.address());
    auto& addrLoad = dyncast<Load const&>(*elemGep.basePointer());
    auto& addrGep = dyncast<GetElementPointer const&>(*addrLoad.address());
    CHECK(addrGep.basePointer() == &F.parameters().front());
}

TEST_CASE("'empty' property", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(n: &[int]) { return n.empty; }" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& cmp = view.nextAs<CompareInst>();
    CHECK(cmp.lhs() == &F.parameters().back());
    CHECK(cmp.rhs() == ctx.intConstant(0, 64));
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &cmp);
}

TEST_CASE("'front' property on register array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() { return get().front; }
fn get() -> [int, 2] {}
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& call = view.nextAs<Call>();
    auto& extract = view.nextAs<ExtractValue>();
    CHECK(extract.baseValue() == &call);
    CHECK(extract.memberIndices().size() == 1);
    CHECK(extract.memberIndices().front() == 0);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &extract);
}

TEST_CASE("'back' property on register array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo() { return get().back; }
fn get() -> [byte, 8] {}
)" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& call = view.nextAs<Call>();
    auto& extract = view.nextAs<ExtractValue>();
    CHECK(extract.baseValue() == &call);
    CHECK(extract.memberIndices().size() == 1);
    CHECK(extract.memberIndices().front() == 7);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &extract);
}

TEST_CASE("'front' property on memory array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(n: &[int]) { return n.front; }" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& gep = view.nextAs<GetElementPointer>();
    CHECK(gep.basePointer() == &F.parameters().front());
    CHECK(gep.arrayIndex() == ctx.intConstant(0, 64));
    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &gep);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &load);
}

TEST_CASE("'back' property on memory array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(n: &[int]) { return n.back; }" });
    auto& F = mod.front();
    auto view = BBView(F.entry());

    auto& sub = view.nextAs<ArithmeticInst>();
    CHECK(sub.lhs() == &F.parameters().back());
    CHECK(sub.rhs() == ctx.intConstant(1, 64));
    auto& gep = view.nextAs<GetElementPointer>();
    CHECK(gep.basePointer() == &F.parameters().front());
    CHECK(gep.arrayIndex() == &sub);
    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &gep);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &load);
}
