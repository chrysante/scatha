#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("IRGen - Local variable of trivial type", "[irgen]") {
    using namespace ir;
    std::string source = GENERATE("public fn foo() { var i: int; }", // No initializer
                                  "public fn foo() { var i = 0; }"); // = 0
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto view = BBView(F.entry());
    
    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == ctx.intType(64));
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &mem);
    CHECK(store.value() == ctx.intConstant(0, 64));
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Local variable copy of trivial type parameter", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(n: int) { let i = n; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());
    
    auto& n_addr = view.nextAs<Alloca>();
    auto& i_addr = view.nextAs<Alloca>();
    auto& store_n = view.nextAs<Store>();
    CHECK(store_n.address() == &n_addr);
    CHECK(store_n.value() == &F.parameters().front());
    auto& load_n = view.nextAs<Load>();
    CHECK(load_n.address() == &n_addr);
    auto& store_i = view.nextAs<Store>();
    CHECK(store_i.address() == &i_addr);
    CHECK(store_i.value() == &load_n);
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Local reference variable to parameter", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(n: &int) { let i: &int = n; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    CHECK(F.entry().emptyExceptTerminator());
}

TEST_CASE("IRGen - Local variable array pointer to array reference argument", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(data: &[int]) { let p = &data; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());
    
    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == arrayPointerType(ctx));
    CHECK(view.nextAs<InsertValue>().insertedValue() == &F.parameters().front());
    auto& p = view.nextAs<InsertValue>();
    CHECK(p.insertedValue() == &F.parameters().back());
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &mem);
    CHECK(store.value() == &p);
    CHECK_NOTHROW(view.nextAs<Return>());
}
