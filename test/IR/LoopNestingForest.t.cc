#include <catch2/catch_test_macros.hpp>

#include <range/v3/algorithm.hpp>
#include <string>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/IRParser.h"
#include "IR/Loop.h"
#include "IR/Module.h"
#include "Util/Set.h"

using namespace scatha;

static constexpr auto names =
    ranges::views::transform([](auto* x) { return x->basicBlock()->name(); });

TEST_CASE("Loop nesting forests", "[ir][opt]") {
    SECTION("1") {
        auto const text = R"(
func void @f() {
  %entry:
    goto label %header.0

  %header.0:               // preds: entry, if.end
    branch i1 1, label %body.0, label %end.0

  %body.0:                 // preds: header.0
    goto label %header.1

  %end.0:                  // preds: header.0
    goto label %header.2

  %header.1:             // preds: body.0, body.1
    branch i1 1, label %body.1, label %end.1

  %body.1:               // preds: header.1
    goto label %header.1

  %end.1:                // preds: header.1
    branch i1 1, label %if.then, label %if.end

  %if.then:                   // preds: end.1
    goto label %if.end

  %if.end:                    // preds: end.1, if.then
    goto label %header.0

  %header.2:             // preds: end.0, body.2
    branch i1 1, label %body.2, label %end.2

  %body.2:               // preds: header.2
    goto label %header.2

  %end.2:                // preds: header.2
    return
})";
        auto [ctx, mod] = ir::parse(text).value();
        auto& F = mod.front();
        auto& LNF = F.getOrComputeLNF();
        auto find = [&](auto&& rng, std::string_view name) {
            return *ranges::find(rng, name, [](auto* n) {
                return n->basicBlock()->name();
            });
        };
        REQUIRE((LNF.roots() | names) ==
                test::Set{ "entry", "header.0", "end.0", "header.2", "end.2" });
        auto* header_0 = find(LNF.roots(), "header.0");
        REQUIRE(
            (header_0->children() | names) ==
            test::Set{ "body.0", "if.then", "if.end", "header.1", "end.1" });
        auto* header_1 = find(header_0->children(), "header.1");
        REQUIRE((header_1->children() | names) == test::Set{ "body.1" });
        auto* header_2 = find(LNF.roots(), "header.2");
        REQUIRE((header_2->children() | names) == test::Set{ "body.2" });
    }
    SECTION("2") {
        auto const text = R"(
func void @f() {
  %entry:
    return
})";
        auto [ctx, mod] = ir::parse(text).value();
        auto& F = mod.front();
        auto& LNF = F.getOrComputeLNF();
        REQUIRE((LNF.roots() | names) == test::Set{ "entry" });
    }
}
