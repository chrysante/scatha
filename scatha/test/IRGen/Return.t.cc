#include <catch2/catch_test_macros.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("Return trivial by value argument", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(value: int) -> int { return value; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());

    auto& allocaInst = view.nextAs<Alloca>();
    CHECK(allocaInst.allocatedType() == ctx.intType(64));
    CHECK(allocaInst.count() == ctx.intConstant(1, 32));

    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &allocaInst);
    CHECK(store.value() == &F.parameters().front());

    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &allocaInst);
    CHECK(load.type() == allocaInst.allocatedType());

    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &load);
}

TEST_CASE("Return trivial by reference argument", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(value: &int) -> int { return value; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());

    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &F.parameters().front());
    CHECK(load.type() == ctx.intType(64));

    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &load);
}

TEST_CASE("Return trivial by pointer argument", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(ptr: *int) -> int { return *ptr; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == ctx.ptrType());
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &mem);
    CHECK(store.value() == &F.parameters().front());
    auto& loadPtr = view.nextAs<Load>();
    CHECK(loadPtr.address() == &mem);
    CHECK(loadPtr.type() == ctx.ptrType());
    auto& loadInt = view.nextAs<Load>();
    CHECK(loadInt.address() == &loadPtr);
    CHECK(loadInt.type() == ctx.intType(64));
    CHECK(view.nextAs<Return>().value() == &loadInt);
}

TEST_CASE("Return count of dynamic array reference", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(data: &[int]) { return data.count; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());

    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &F.parameters().back());
}

TEST_CASE("Return count of dynamic array pointer", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(data: *[int]) { return data.count; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == arrayPointerType(ctx));
    view.nextAs<InsertValue>();
    auto& packed = view.nextAs<InsertValue>();
    CHECK(view.nextAs<Store>().value() == &packed);
    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &mem);
    auto& size = view.nextAs<ExtractValue>();
    CHECK(size.baseValue() == &load);
    CHECK(view.nextAs<Return>().value() == &size);
}

TEST_CASE("Return count of reference to dynamic array pointer", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(data: &*[int]) { return data.count; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());

    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &F.parameters().front());
    auto& size = view.nextAs<ExtractValue>();
    CHECK(size.baseValue() == &load);
    CHECK(view.nextAs<Return>().value() == &size);
}

TEST_CASE("Pass reference to dynamic array through function", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(ref: &[int]) -> &[int] { return ref; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());

    auto& insert1 = view.nextAs<InsertValue>();
    CHECK(insert1.insertedValue() == &F.parameters().front());
    auto& insert2 = view.nextAs<InsertValue>();
    CHECK(insert2.insertedValue() == &F.parameters().back());
    CHECK(view.nextAs<Return>().value() == &insert2);
}

/// TODO: Move to assign.t file
TEST_CASE("Assign to reference to dynamic array pointer", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(p: &mut *[int], q: *[int]) { p = q; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 3);
    auto view = BBView(F.entry());

    CHECK_NOTHROW(view.nextAs<Alloca>());
    CHECK_NOTHROW(view.nextAs<InsertValue>());
    CHECK_NOTHROW(view.nextAs<InsertValue>());
    CHECK_NOTHROW(view.nextAs<Store>());
    auto& q = view.nextAs<Load>();
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &F.parameters().front());
    CHECK(store.value() == &q);
    CHECK_NOTHROW(view.nextAs<Return>());
}
