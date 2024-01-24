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

TEST_CASE("Lifetime operation analysis", "[sema][lifetime]") {
    auto const text = R"(
public struct Empty {}
public struct Trivial { fn new(&mut this) {} }
public struct Nontrivial { fn delete(&mut this) {} }
public struct Nontrivial2 { 
    fn new(&mut this, rhs: &Nontrivial2) {}
    fn delete(&mut this) {}
}
public struct WithNontrivMember {
    var nontriv: Nontrivial2;
}
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);

    SECTION("Empty") {
        auto* type = lookup<StructType>(sym, "Empty");
        CHECK(type->hasTrivialLifetime());
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        CHECK(md->defaultConstructor().isTrivial());
        CHECK(md->copyConstructor().isTrivial());
        CHECK(md->moveConstructor().isTrivial());
        CHECK(md->destructor().isTrivial());
    }

    SECTION("Trivial") {
        auto* type = lookup<StructType>(sym, "Trivial");
        CHECK(type->hasTrivialLifetime());
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        REQUIRE(md->defaultConstructor().function());
        CHECK(md->defaultConstructor().function()->isNative());
        CHECK(md->copyConstructor().isTrivial());
        CHECK(md->moveConstructor().isTrivial());
        CHECK(md->destructor().isTrivial());
    }

    SECTION("Trivial array") {
        auto* elemType = lookup<StructType>(sym, "Trivial");
        auto* type = sym.arrayType(elemType, 2);
        CHECK(type->hasTrivialLifetime());
        using enum LifetimeOperation::Kind;
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        CHECK(md->defaultConstructor().kind() == NontrivialInline);
        CHECK(md->copyConstructor().isTrivial());
        CHECK(md->moveConstructor().isTrivial());
        CHECK(md->destructor().isTrivial());
    }

    SECTION("Nontrivial") {
        auto* type = lookup<StructType>(sym, "Nontrivial");
        CHECK(!type->hasTrivialLifetime());
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        CHECK(md->defaultConstructor().isDeleted());
        CHECK(md->copyConstructor().isDeleted());
        CHECK(md->moveConstructor().isDeleted());
        CHECK(md->destructor().function());
    }

    SECTION("Nontrivial2") {
        auto* type = lookup<StructType>(sym, "Nontrivial2");
        CHECK(!type->hasTrivialLifetime());
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        CHECK(md->defaultConstructor().isDeleted());
        REQUIRE(md->copyConstructor().function());
        CHECK(md->copyConstructor().function()->isNative());
        CHECK(md->moveConstructor().isDeleted());
        REQUIRE(md->destructor().function());
        CHECK(md->destructor().function()->isNative());
    }

    SECTION("Nontrivial2 array") {
        auto* elemType = lookup<StructType>(sym, "Nontrivial2");
        auto* type = sym.arrayType(elemType, 2);
        CHECK(!type->hasTrivialLifetime());
        using enum LifetimeOperation::Kind;
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        CHECK(md->defaultConstructor().isDeleted());
        CHECK(md->copyConstructor().kind() == NontrivialInline);
        CHECK(md->moveConstructor().isDeleted());
        CHECK(md->destructor().kind() == NontrivialInline);
    }

    SECTION("WithNontrivMember") {
        auto* type = lookup<StructType>(sym, "WithNontrivMember");
        CHECK(!type->hasTrivialLifetime());
        auto* md = type->lifetimeMetadata();
        REQUIRE(md);
        CHECK(md->defaultConstructor().isDeleted());
        REQUIRE(md->copyConstructor().function());
        CHECK(md->copyConstructor().function()->isGenerated());
        CHECK(md->moveConstructor().isDeleted());
        REQUIRE(md->destructor().function());
        CHECK(md->destructor().function()->isGenerated());
    }
}
