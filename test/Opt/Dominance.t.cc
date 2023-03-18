#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "Opt/Dominance.h"

using namespace scatha;

TEST_CASE("Dominance - 1", "[opt]") {
    ir::Context ctx;
    auto const text = R"(
function i64 @f() {
  %entry:
    goto label %2
  %2:
    %cond = cmp leq i64 $1, i64 $2
    branch i1 %cond, label %3, label %4
  %3:
    goto label %5
  %4:
    goto label %5
  %5:
    goto label %6
  %6:
    goto label %7
  %7:
    branch i1 %cond, label %8, label %6
  %8:
    return i64 $0
})";
    ir::Module mod  = ir::parse(text, ctx).value();
    auto& f         = mod.functions().front();
    auto domTree    = opt::buildDomTree(f);
    auto& root      = domTree.root();
    CHECK(root.basicBlock()->name() == "entry");
    auto& BB2 = root.children()[0];
    CHECK(BB2.basicBlock()->name() == "2");
    auto childrenOf2  = BB2.children();
    auto findChildOf2 = [&](std::string_view name) {
        return ranges::find_if(childrenOf2, [&](auto& node) {
            return node.basicBlock()->name() == name;
        });
    };
    auto isChildOf2 = [&](std::string_view name) {
        return findChildOf2(name) != ranges::end(childrenOf2);
    };
    REQUIRE(isChildOf2("3"));
    REQUIRE(isChildOf2("4"));
    REQUIRE(isChildOf2("5"));
    auto& BB3 = *findChildOf2("3");
    auto& BB4 = *findChildOf2("4");
    auto& BB5 = *findChildOf2("5");
    auto& BB6 = BB5.children()[0];
    CHECK(BB6.basicBlock()->name() == "6");
    auto& BB7 = BB6.children()[0];
    CHECK(BB7.basicBlock()->name() == "7");
    auto& BB8 = BB7.children()[0];
    CHECK(BB8.basicBlock()->name() == "8");
    /// ## Dominance frontiers
    auto dfMap = opt::computeDominanceFrontiers(f, domTree);
    auto df    = [&](auto& node) -> auto& {
        return dfMap.find(node.basicBlock())->second;
    };
    CHECK(df(root).empty());
    CHECK(df(BB2).empty());
    CHECK(df(BB3).front() == BB5.basicBlock());
    CHECK(df(BB4).front() == BB5.basicBlock());
    CHECK(df(BB5).empty());
    CHECK(df(BB6).front() == BB6.basicBlock());
    CHECK(df(BB7).front() == BB6.basicBlock());
    CHECK(df(BB8).empty());
}
