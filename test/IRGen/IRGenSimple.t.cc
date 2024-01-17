#include <catch2/catch_test_macros.hpp>

#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"

using namespace scatha;
using test::makeIR;

TEST_CASE("IRGen - Return trivial by value argument", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(value: int) -> int { return value; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto itr = F.front().begin();
    
    auto& allocaInst = dyncast<Alloca const&>(*itr++);
    CHECK(allocaInst.allocatedType() == ctx.intType(64));
    CHECK(allocaInst.count() == ctx.intConstant(1, 32));
    
    auto& store = dyncast<Store const&>(*itr++);
    CHECK(store.address() == &allocaInst);
    CHECK(store.value() == &F.parameters().front());
    
    auto& load = dyncast<Load const&>(*itr++);
    CHECK(load.address() == &allocaInst);
    CHECK(load.type() == allocaInst.allocatedType());
    
    auto& ret = dyncast<Return const&>(*itr++);
    CHECK(ret.value() == &load);
}

TEST_CASE("IRGen - Return trivial by reference argument", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(value: &int) -> int { return value; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto itr = F.front().begin();
    
    auto& load = dyncast<Load const&>(*itr++);
    CHECK(load.address() == &F.parameters().front());
    CHECK(load.type() == ctx.intType(64));
    
    auto& ret = dyncast<Return const&>(*itr++);
    CHECK(ret.value() == &load);
}
