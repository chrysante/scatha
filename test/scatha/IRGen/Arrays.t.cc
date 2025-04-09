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

TEST_CASE("Statically generated list expression", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo() { let data = [1, 2, 3]; }" });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedSize().value() == 3 * 8);
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

TEST_CASE("Dynamically generated list expression", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(data: &[int]) { let arr = [&data]; }" });
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

TEST_CASE("Default constructed small local array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo() { let data: [int, 2]; }" });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedSize().value() == 2 * 8);
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &mem);
    CHECK(store.value() ==
          ctx.arrayConstant(std::array<Constant*, 2>{ ctx.intConstant(0, 64),
                                                      ctx.intConstant(0, 64) },
                            ctx.arrayType(ctx.intType(64), 2)));
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("Default constructed big local array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ "public fn foo() { let data: [int, 10]; }" });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedSize().value() == 10 * 8);
    auto& memset = view.nextAs<Call>();
    CHECK(memset.function()->name() == "__builtin_memset");
    CHECK(memset.argumentAt(0) == &mem);
    CHECK(memset.argumentAt(1) == ctx.intConstant(80, 64));
    CHECK(memset.argumentAt(2) == ctx.intConstant(0, 64));
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("Copy constructed small local array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(data: &[int, 2]) { let data2 = data; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == ctx.arrayType(ctx.intType(64), 2));
    CHECK(mem.count() == ctx.intConstant(1, 32));
    auto& load = view.nextAs<Load>();
    CHECK(load.address() == &F.parameters().front());
    auto& store = view.nextAs<Store>();
    CHECK(store.address() == &mem);
    CHECK(store.value() == &load);
    CHECK_NOTHROW(view.nextAs<Return>());
}

TEST_CASE("Copy constructed big local array", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(data: &[int, 8]) { let data2 = data; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 1);
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedSize().value() == 8 * 8);
    auto& memcpy = view.nextAs<Call>();
    CHECK(memcpy.function()->name() == "__builtin_memcpy");
    CHECK(memcpy.argumentAt(0) == &mem);
    CHECK(memcpy.argumentAt(1) == ctx.intConstant(64, 64));
    CHECK(memcpy.argumentAt(2) == &F.parameters().front());
    CHECK_NOTHROW(view.nextAs<Return>());
}
