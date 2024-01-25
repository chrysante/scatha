#include <catch2/catch_test_macros.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("Count of dynamic array pointer in dynamic array",
          "[irgen]") {
    using namespace ir;
    auto [ctx, mod] =
        makeIR({ "public fn foo(p: *[*[int]]) -> int { return p[0].count; }" });
    auto& F = mod.front();
    CHECK(ranges::distance(F.parameters()) == 2);
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    CHECK(mem.allocatedType() == arrayPointerType(ctx));
    CHECK(view.nextAs<InsertValue>().insertedValue() ==
          &F.parameters().front());
    auto& p = view.nextAs<InsertValue>();
    CHECK(p.insertedValue() == &F.parameters().back());
    CHECK(view.nextAs<Store>().value() == &p);
    auto& p2 = view.nextAs<Load>();
    auto& p2_data = view.nextAs<ExtractValue>();
    CHECK(p2_data.baseValue() == &p2);
    auto& p2_count = view.nextAs<ExtractValue>();
    CHECK(p2_count.baseValue() == &p2);
    auto& p2_at0_addr = view.nextAs<GetElementPointer>();
    CHECK(p2_at0_addr.basePointer() == &p2_data);
    auto& p2_at0 = view.nextAs<Load>();
    CHECK(p2_at0.address() == &p2_at0_addr);
    auto& p2_at0_data = view.nextAs<ExtractValue>();
    CHECK(p2_at0_data.baseValue() == &p2_at0);
    auto& p2_at0_count = view.nextAs<ExtractValue>();
    CHECK(p2_at0_count.baseValue() == &p2_at0);
    CHECK(view.nextAs<Return>().value() == &p2_at0_count);
}
