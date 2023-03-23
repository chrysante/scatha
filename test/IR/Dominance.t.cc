#include <Catch/Catch2.hpp>

#include <array>

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "IR/Dominance.h"

using namespace scatha;

static auto& find(auto range, std::string_view name) {
    auto itr = ranges::find_if(range, [&](auto& node) {
        return node.basicBlock()->name() == name;
    });
    assert(itr != ranges::end(range));
    return *itr;
};

/// Requires the sequences \p a and \p b to be unique.
static bool setEqual(auto&& a, auto&& b) {
    if (ranges::size(a) != ranges::size(b)) {
        return false;
    }
    for (auto&& x: a) {
        if (ranges::find(b, x) == ranges::end(b)) {
            return false;
        }
    }
    return true;
}

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
    auto domInfo    = ir::DominanceInfo::compute(f);
    /// ## Dominator tree
    auto& domTree = domInfo.domTree();
    auto& root    = domTree.root();
    CHECK(root.basicBlock()->name() == "entry");
    REQUIRE(root.children().size() == 1);
    auto& BB2 = root.children()[0];
    CHECK(BB2.basicBlock()->name() == "2");
    auto childrenOf2 = BB2.children();
    auto& BB3        = find(childrenOf2, "3");
    REQUIRE(BB3.children().empty());
    auto& BB4 = find(childrenOf2, "4");
    REQUIRE(BB4.children().empty());
    auto& BB5 = find(childrenOf2, "5");
    REQUIRE(BB5.children().size() == 1);
    auto& BB6 = BB5.children()[0];
    CHECK(BB6.basicBlock()->name() == "6");
    REQUIRE(BB6.children().size() == 1);
    auto& BB7 = BB6.children()[0];
    CHECK(BB7.basicBlock()->name() == "7");
    REQUIRE(BB7.children().size() == 1);
    auto& BB8 = BB7.children()[0];
    CHECK(BB8.basicBlock()->name() == "8");
    REQUIRE(BB8.children().empty());
    /// ## Dominance frontiers
    auto df = [&](auto& node) { return domInfo.domFront(node.basicBlock()); };
    CHECK(df(root).empty());
    CHECK(df(BB2).empty());
    CHECK(setEqual(df(BB3), std::array{ BB5.basicBlock() }));
    CHECK(setEqual(df(BB4), std::array{ BB5.basicBlock() }));
    CHECK(df(BB5).empty());
    CHECK(setEqual(df(BB6), std::array{ BB6.basicBlock() }));
    CHECK(setEqual(df(BB7), std::array{ BB6.basicBlock() }));
    CHECK(df(BB8).empty());
}

TEST_CASE("Dominance - 2", "[opt]") {
    ir::Context ctx;
    auto const text = R"(
function i64 @f() {
  %entry:
    %cond = cmp leq i64 $1, i64 $2
    branch i1 %cond, label %1, label %2
  %1:
    goto label %3
  %2:
    goto label %4
  %3:
    branch i1 %cond, label %1, label %4
  %4:
    return i64 $0
})";
    ir::Module mod  = ir::parse(text, ctx).value();
    auto& f         = mod.functions().front();
    auto domInfo    = ir::DominanceInfo::compute(f);
    /// ## Dominator tree
    auto& domTree = domInfo.domTree();
    auto& root    = domTree.root();
    CHECK(root.basicBlock()->name() == "entry");
    REQUIRE(root.children().size() == 3);
    auto childrenOfRoot = root.children();
    auto& BB1           = find(childrenOfRoot, "1");
    auto& BB2           = find(childrenOfRoot, "2");
    REQUIRE(BB2.children().empty());
    auto& BB4 = find(childrenOfRoot, "4");
    REQUIRE(BB1.children().size() == 1);
    auto& BB3 = BB1.children()[0];
    REQUIRE(BB3.children().empty());
    /// ## Dominance frontiers
    auto df = [&](auto& node) { return domInfo.domFront(node.basicBlock()); };
    CHECK(df(root).empty());
    CHECK(setEqual(df(BB1), std::array{ BB1.basicBlock(), BB4.basicBlock() }));
    CHECK(setEqual(df(BB2), std::array{ BB4.basicBlock() }));
    CHECK(setEqual(df(BB3), std::array{ BB1.basicBlock(), BB4.basicBlock() }));
    CHECK(df(BB4).empty());
}
