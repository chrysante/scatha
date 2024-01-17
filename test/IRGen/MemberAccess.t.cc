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

TEST_CASE("IRGen - Member access simple", "[irgen]") {
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

TEST_CASE("IRGen - Member access dynamic array pointer member", "[irgen]") {
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
