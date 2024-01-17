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

TEST_CASE("IRGen - Statically generated list expression", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo() { let data = [1, 2, 3]; }" });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto view = BBView(F.entry());
    
    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == ctx.intType(64));
    CHECK(mem.count() == ctx.intConstant(3, 32));
    auto& memcpy = view.nextAs<Call>();
    CHECK(memcpy.function()->name() == "__builtin_memcpy");
    CHECK(memcpy.argumentAt(0) == &mem);
    CHECK(memcpy.argumentAt(1) == ctx.intConstant(24, 64));
    auto& global = dyncast<GlobalVariable const&>(*memcpy.argumentAt(2));
    auto& data = dyncast<ArrayConstant const&>(*global.initializer());
    CHECK(data.elementAt(0) == ctx.intConstant(1, 64));
    CHECK(data.elementAt(1) == ctx.intConstant(2, 64));
    CHECK(data.elementAt(2) == ctx.intConstant(3, 64));
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("IRGen - Dynamically generated list expression", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo(data: &[int]) { let arr = [&data]; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());
    
    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == arrayPointerType(ctx));
    CHECK(mem.count() == ctx.intConstant(1, 32));
    
    auto& gep = view.nextAs<GetElementPointer>();
    CHECK(gep.basePointer() == &mem);
    CHECK(gep.arrayIndex() == ctx.intConstant(0, 32));
    view.nextAs<InsertValue>();
    auto& arrayPtr = view.nextAs<InsertValue>();
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &gep);
    CHECK(store.value() == &arrayPtr);
    CHECK_NOTHROW(view.nextAs<Return>());
}
