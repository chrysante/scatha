#include <array>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/Utility.h"
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
public struct Trivial { 
    fn new(&mut this) {}
    var i: int;
}
public struct Nontrivial {
    fn delete(&mut this) {}
    var i: int;
}
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
        auto& md = type->lifetimeMetadata();
        CHECK(md.defaultConstructor().isTrivial());
        CHECK(md.copyConstructor().isTrivial());
        CHECK(md.moveConstructor().isTrivial());
        CHECK(md.destructor().isTrivial());
    }

    SECTION("Trivial") {
        auto* type = lookup<StructType>(sym, "Trivial");
        CHECK(type->hasTrivialLifetime());
        auto& md = type->lifetimeMetadata();
        REQUIRE(md.defaultConstructor().function());
        CHECK(md.defaultConstructor().function()->isNative());
        CHECK(md.copyConstructor().isTrivial());
        CHECK(md.moveConstructor().isTrivial());
        CHECK(md.destructor().isTrivial());
    }

    SECTION("Trivial array") {
        auto* elemType = lookup<StructType>(sym, "Trivial");
        auto* type = sym.arrayType(elemType, 2);
        CHECK(type->hasTrivialLifetime());
        using enum LifetimeOperation::Kind;
        auto& md = type->lifetimeMetadata();
        CHECK(md.defaultConstructor().kind() == NontrivialInline);
        CHECK(md.copyConstructor().isTrivial());
        CHECK(md.moveConstructor().isTrivial());
        CHECK(md.destructor().isTrivial());
    }

    SECTION("Nontrivial") {
        auto* type = lookup<StructType>(sym, "Nontrivial");
        CHECK(!type->hasTrivialLifetime());
        auto& md = type->lifetimeMetadata();
        CHECK(md.defaultConstructor().isDeleted());
        CHECK(md.copyConstructor().isDeleted());
        CHECK(md.moveConstructor().isDeleted());
        CHECK(md.destructor().function());
    }

    SECTION("Nontrivial2") {
        auto* type = lookup<StructType>(sym, "Nontrivial2");
        CHECK(!type->hasTrivialLifetime());
        auto& md = type->lifetimeMetadata();
        CHECK(md.defaultConstructor().isDeleted());
        REQUIRE(md.copyConstructor().function());
        CHECK(md.copyConstructor().function()->isNative());
        CHECK(md.moveConstructor().isDeleted());
        REQUIRE(md.destructor().function());
        CHECK(md.destructor().function()->isNative());
    }

    SECTION("Nontrivial2 array") {
        auto* elemType = lookup<StructType>(sym, "Nontrivial2");
        auto* type = sym.arrayType(elemType, 2);
        CHECK(!type->hasTrivialLifetime());
        using enum LifetimeOperation::Kind;
        auto& md = type->lifetimeMetadata();
        CHECK(md.defaultConstructor().isDeleted());
        CHECK(md.copyConstructor().kind() == NontrivialInline);
        CHECK(md.moveConstructor().isDeleted());
        CHECK(md.destructor().kind() == NontrivialInline);
    }

    SECTION("WithNontrivMember") {
        auto* type = lookup<StructType>(sym, "WithNontrivMember");
        CHECK(!type->hasTrivialLifetime());
        auto& md = type->lifetimeMetadata();
        CHECK(md.defaultConstructor().isDeleted());
        REQUIRE(md.copyConstructor().function());
        CHECK(md.copyConstructor().function()->isGenerated());
        CHECK(md.moveConstructor().isDeleted());
        REQUIRE(md.destructor().function());
        CHECK(md.destructor().function()->isGenerated());
    }
}

TEST_CASE("Non-aggregate object construction AST rewrites",
          "[sema][lifetime]") {
    auto const text = R"(
struct Trivial {
    fn new(&mut this) {}
    fn new(&mut this, n: int) {}
}
fn foo() {
    var t: Trivial;
    var s = Trivial(1);
    var r = t;
}
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());

    auto* file = cast<TranslationUnit*>(ast.get())->sourceFile(0);

    /// Struct definition
    auto* trivial = file->statement<StructDefinition>(0);
    auto* defCtor = trivial->body()->statement<FunctionDefinition>(0);
    auto* intCtor = trivial->body()->statement<FunctionDefinition>(1);

    /// Function definition
    auto* foo = file->statement<FunctionDefinition>(1);

    auto* t = foo->body()->statement<VariableDeclaration>(0);
    auto* tConstr = t->initExpr<NontrivConstructExpr>();
    CHECK(tConstr->constructor() == defCtor->function());

    auto* s = foo->body()->statement<VariableDeclaration>(1);
    auto* sConstr = s->initExpr<NontrivConstructExpr>();
    CHECK(sConstr->constructor() == intCtor->function());

    auto* r = foo->body()->statement<VariableDeclaration>(2);
    auto* rConstr = r->initExpr<TrivCopyConstructExpr>();
    CHECK(rConstr->CallLike::argument<Identifier>(0)->entity() == t->entity());
}

TEST_CASE("Aggregates", "[sema][lifetime]") {
    auto const text = R"(
public struct Empty {}
public struct DefCtor {
    fn new(&mut this) {}
    var i: int;
}
public struct Nontrivial {
    fn new(&mut this, rhs: &Nontrivial) {}
    fn delete(&mut this) {}
}
public struct NontrivMember {
    var nontriv: Nontrivial;
}
public struct PrivateMember {
    private var n: int;
}
public struct InternalMember {
    internal var n: int;
}
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);

    auto* Empty = lookup<StructType>(sym, "Empty");
    CHECK(isAggregate(Empty));
    auto* DefCtor = lookup<StructType>(sym, "DefCtor");
    CHECK(!isAggregate(DefCtor));
    auto* Nontrivial = lookup<StructType>(sym, "Nontrivial");
    CHECK(!isAggregate(Nontrivial));
    auto* NontrivMember = lookup<StructType>(sym, "NontrivMember");
    CHECK(isAggregate(NontrivMember));
    auto* PrivateMember = lookup<StructType>(sym, "PrivateMember");
    CHECK(!isAggregate(PrivateMember));
    auto* InternalMember = lookup<StructType>(sym, "InternalMember");
    CHECK(!isAggregate(InternalMember));
}
