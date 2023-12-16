#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "Sema/Serialize.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using namespace test;

namespace {

struct Finder {
    SymbolTable const& sym;

    Entity const* findImpl(std::string_view name) const {
        auto entities = sym.currentScope().findEntities(name);
        if (!entities.empty()) {
            return entities.front();
        }
        return nullptr;
    }

    Scope const* operator()(std::string_view name, auto fn) const {
        auto* entity = findImpl(name);
        if (!entity) {
            throw std::runtime_error("Failed to find \"" + std::string(name) +
                                     "\"");
        }
        auto* scope = dyncast<Scope const*>(entity);
        if (!scope) {
            throw std::runtime_error("\"" + std::string(name) +
                                     "\" is not a scope");
        }
        sym.withScopePushed(scope, [&] { fn(scope); });
        return scope;
    }

    Entity const* operator()(std::string_view name) const {
        return findImpl(name);
    }
};

} // namespace

TEST_CASE("Symbol table serialize/deserialize", "[sema]") {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(R"(
struct X {
    fn foo(n: int) -> double {}
    fn bar(&this, ptr: *unique mut int) {}
    var baz: [Y, 2];
    struct Y { var k: int; }
}
struct Y {}
)");
    REQUIRE(iss.empty());
    std::stringstream sstr;
    serialize(sym, sstr);
    SymbolTable sym2;
    deserialize(sym2, sstr);
    Finder find{ sym2 };
    find("X", [&](Scope const* X) {
        CHECK(cast<Type const*>(X)->size() == 2 * sym2.Int()->size());
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
        auto* baz = dyncast<Variable const*>(find("baz"));
        REQUIRE(baz);
        auto* bazType = dyncast<ArrayType const*>(baz->type());
        REQUIRE(bazType);
        CHECK(bazType->elementType() == Y);
        CHECK(bazType->count() == 2);
    });
    CHECK(cast<Type const*>(find("Y"))->size() == 1);
}

TEST_CASE("Symbol table empty deserialization", "[sema]") {
    std::stringstream sstr;
    sstr << "{}";
    SymbolTable sym;
    CHECK(deserialize(sym, sstr));
}

TEST_CASE("Symbol table erroneous deserialization", "[sema]") {
    std::stringstream sstr;
    sstr << "random nonesense";
    SymbolTable sym;
    CHECK(!deserialize(sym, sstr));
}
