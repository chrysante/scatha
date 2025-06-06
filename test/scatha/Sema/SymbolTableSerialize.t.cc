#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/SemaUtil.h"
#include "Sema/Serialize.h"
#include "Sema/SimpleAnalzyer.h"
#include "Sema/VTable.h"

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
public protocol P {
    fn test(&this) -> void;
}
public protocol P2 {}
public struct Base1 {}
public struct Base2 {}
public struct Dyn: P, P2, Base1, Base2 {
    fn test(&dyn this) -> void {}
    var n: int;
}
public fn dynArgFn(arg: &dyn mut Dyn) {}
)");
    REQUIRE(iss.empty());
    sym.prepareExport();
    std::stringstream sstr;
    serialize(sym, sstr);
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
        auto* quuz = X->memberVariables().back();
        REQUIRE(quuz);
        CHECK(quuz->name() == "quux");
        CHECK(quuz->index() == 1);
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
    auto* P = find("P", [&](Scope const* PScope) {
        auto& P = dyncast<ProtocolType const&>(*PScope);
        REQUIRE(P.vtable());
        auto* pTest = dyncast<Function const*>(find("test"));
        REQUIRE(pTest);
        auto& VT = *P.vtable();
        CHECK(VT.sortedInheritedVTables().empty());
        REQUIRE(VT.layout().size() == 1);
        CHECK(VT.layout().front() == pTest);
    });
    auto* Dyn = find("Dyn", [&](Scope const* DynScope) {
        auto& Dyn = dyncast<StructType const&>(*DynScope);
        CHECK(Dyn.memberVariables().size() == 1);
        CHECK(Dyn.baseStructObjects().size() == 2);
        CHECK(Dyn.conformingProtocolObjects().size() == 2);
        CHECK(ranges::contains(Dyn.baseTypes(), P));
        REQUIRE(Dyn.vtable());
        auto* dynTest = dyncast<Function const*>(find("test"));
        REQUIRE(dynTest);
        auto& VT = *Dyn.vtable();
        auto inherited = VT.sortedInheritedVTables();
        REQUIRE(inherited.size() == 4);
        CHECK(inherited[0]->correspondingType() == P);
        CHECK(inherited[1]->correspondingType()->name() == "P2");
        CHECK(inherited[2]->correspondingType()->name() == "Base1");
        CHECK(inherited[3]->correspondingType()->name() == "Base2");
        REQUIRE(inherited[0]->layout().size() == 1);
        CHECK(inherited[0]->correspondingType() == P);
        CHECK(VT.layout().empty());
    });
    find("dynArgFn", [&](Scope const* scope) {
        auto& dynArgFn = dyncast<Function const&>(*scope);
        REQUIRE(dynArgFn.argumentCount() == 1);
        auto* argType = dyncast<ReferenceType const*>(dynArgFn.argumentType(0));
        REQUIRE(argType);
        CHECK(argType->base().get() == Dyn);
        CHECK(argType->base().isMut());
        CHECK(argType->base().isDyn());
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
