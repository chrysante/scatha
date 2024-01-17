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

TEST_CASE("IRGen - Dynamic array reference in conditional expression",
          "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(a: &[int], b: &[int]) -> &[int] { return true ? a : b; }
)" });
    auto& F = mod.front();
    CHECK(ranges::distance(F) == 4);
    auto itr = F.begin();
    CHECK(itr++->emptyExceptTerminator());
    CHECK(itr++->emptyExceptTerminator());
    CHECK(itr++->emptyExceptTerminator());
    auto view = BBView(*itr);

    auto& phiData = view.nextAs<Phi>();
    auto& phiCount = view.nextAs<Phi>();
    auto& insertData = view.nextAs<InsertValue>();
    CHECK(insertData.insertedValue() == &phiData);
    auto& insertCount = view.nextAs<InsertValue>();
    CHECK(insertCount.insertedValue() == &phiCount);
    auto& ret = view.nextAs<Return>();
    CHECK(ret.value() == &insertCount);
}

TEST_CASE("IRGen - .count on conditional expression", "[irgen]") {
    using namespace ir;
    auto [ctx, mod] = makeIR({ R"(
public fn foo(a: &[int], b: &[int]) { return (true ? a : b).count; }
)" });
    auto& F = mod.front();
    CHECK(ranges::distance(F) == 4);
    auto itr = F.begin();
    CHECK(itr++->emptyExceptTerminator());
    CHECK(itr++->emptyExceptTerminator());
    CHECK(itr++->emptyExceptTerminator());
    auto& exit = *itr;
    auto& ret = *exit.terminator<Return>();
    auto& phi = dyncast<Phi const&>(*ret.value());
    CHECK(phi.parent() == ret.parent());
}
