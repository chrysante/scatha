#include <catch2/catch_test_macros.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("Parameter generation", "[irgen]") {
    using namespace ir;
    SECTION("Static array pointer") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: *[int, 3]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        auto itr = F.front().begin();

        auto& allocaInst = dyncast<Alloca const&>(*itr++);
        CHECK(allocaInst.allocatedType() == ctx.ptrType());
        CHECK(allocaInst.count() == ctx.intConstant(1, 32));

        auto& store = dyncast<Store const&>(*itr++);
        CHECK(store.address() == &allocaInst);
        CHECK(store.value() == &F.parameters().front());

        CHECK(isa<Return>(*itr++));
    }
    SECTION("Dynamic array pointer") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: *[int]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 2);
        auto itr = F.front().begin();

        auto& allocaInst = dyncast<Alloca const&>(*itr++);
        CHECK(allocaInst.allocatedType() == arrayPointerType(ctx));
        CHECK(allocaInst.count() == ctx.intConstant(1, 32));

        CHECK(isa<InsertValue>(*itr++));

        auto& packedValue = dyncast<InsertValue const&>(*itr++);
        CHECK(packedValue.type() == arrayPointerType(ctx));

        auto& store = dyncast<Store const&>(*itr++);
        CHECK(store.address() == &allocaInst);
        CHECK(store.value() == &packedValue);

        CHECK(isa<Return>(*itr++));
    }
    SECTION("Big object") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: [int, 10]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.entry().emptyExceptTerminator());
    }
    SECTION("Reference to int") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: &int) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.entry().emptyExceptTerminator());
    }
    SECTION("Reference to dynamic array") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: &[int]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 2);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.parameters().back().type() == ctx.intType(64));
        CHECK(F.entry().emptyExceptTerminator());
    }
    SECTION("Reference to dynamic array pointer") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: &*[int]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.entry().emptyExceptTerminator());
    }
}
