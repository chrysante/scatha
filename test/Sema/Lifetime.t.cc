#include <array>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SemaUtil.h"
#include "Sema/SimpleAnalzyer.h"
#include "Util/IssueHelper.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace sema;
using namespace ast;
using enum ValueCategory;
using test::find;
using test::lookup;

TEST_CASE("Has trivial lifetime", "[sema][lifetime]") {
    auto const text = R"(
public struct Y {}
public struct X { fn new(&mut this) {} }
public struct X { fn delete() {} }
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
}
