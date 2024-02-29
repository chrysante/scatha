#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/SemaUtil.h"
#include "Sema/Serialize.h"
#include "Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using namespace test;

TEST_CASE("Symbol table serialize/deserialize", "[sema]") {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(R"(
public struct X {
    struct Y { var k: int; }

    fn foo(n: int) -> double {}
    fn bar(&this, ptr: *unique mut int) {}

    var baz: [Y, 2];
    var quux: int;
}
public struct Empty {}
public struct Lifetime {
    fn new(&mut this) {}
    fn move(&mut this, rhs: &mut Lifetime) {}
    fn delete(&mut this) {}
}
)");
    REQUIRE(iss.empty());
    std::stringstream sstr;
    serializeLibrary(sym, sstr);
    SymbolTable sym2;
    REQUIRE(deserialize(sym2, sstr));
    Finder find{ sym2 };
    find("X", [&](Scope const* XScope) {
        auto* X = dyncast<StructType const*>(XScope);
        REQUIRE(X);
        CHECK(X->size() == 3 * sym2.Int()->size());
        auto* foo = dyncast<Function const*>(find("foo"));
        REQUIRE(foo);
        REQUIRE(foo->argumentCount() == 1);
        CHECK(foo->argumentType(0) == sym2.Int());
        CHECK(foo->returnType() == sym2.Double());
        auto* bar = dyncast<Function const*>(find("bar"));
        REQUIRE(bar);
        REQUIRE(bar->argumentCount() == 2);
        /* argument 0 */ {
            auto* arg = dyncast<ReferenceType const*>(bar->argumentType(0));
            REQUIRE(arg);
            CHECK(arg->base().isConst());
            CHECK(arg->base().get() == X);
        }
        /* argument 1 */ {
            auto* arg = dyncast<UniquePtrType const*>(bar->argumentType(1));
            REQUIRE(arg);
            CHECK(arg->base().isMut());
            CHECK(arg->base().get() == sym2.Int());
        }
        CHECK(bar->returnType() == sym2.Void());
        auto* Y = find("Y", [&](Scope const* Y) {
            CHECK(cast<Type const*>(Y)->size() == sym2.Int()->size());
            auto* k = dyncast<Variable const*>(find("k"));
            REQUIRE(k);
            CHECK(k->type() == sym2.Int());
        });
        REQUIRE(X->memberVariables().size() == 2);
        CHECK(X->memberVariables().front()->name() == "baz");
        CHECK(X->memberVariables().back()->name() == "quux");
        auto* baz = dyncast<Variable const*>(find("baz"));
        REQUIRE(baz);
        auto* bazType = dyncast<ArrayType const*>(baz->type());
        REQUIRE(bazType);
        CHECK(bazType->elementType() == Y);
        CHECK(bazType->count() == 2);
    });
    CHECK(cast<Type const*>(find("Empty"))->size() == 1);
    find("Lifetime", [&](Scope const* LScope) {
        auto& L = dyncast<StructType const&>(*LScope);
        auto& md = L.lifetimeMetadata();
        using enum LifetimeOperation::Kind;

        auto defCtor = md.defaultConstructor();
        CHECK(defCtor.kind() == Nontrivial);
        CHECK(defCtor.function() == find("new"));

        auto copyCtor = md.copyConstructor();
        CHECK(copyCtor.isDeleted());

        auto moveCtor = md.moveConstructor();
        CHECK(moveCtor.kind() == Nontrivial);
        CHECK(moveCtor.function() == find("move"));

        auto dtor = md.destructor();
        CHECK(dtor.kind() == Nontrivial);
        CHECK(dtor.function() == find("delete"));
    });
}

TEST_CASE("Symbol table empty deserialization", "[sema]") {
    std::stringstream sstr;
    sstr << "{ \"entities\": [] }";
    SymbolTable sym;
    CHECK(deserialize(sym, sstr));
}

TEST_CASE("Symbol table erroneous deserialization", "[sema]") {
    std::stringstream sstr;
    sstr << "random nonsense";
    SymbolTable sym;
    CHECK(!deserialize(sym, sstr));
}
