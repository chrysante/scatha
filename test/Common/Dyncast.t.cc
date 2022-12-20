#include <Catch/Catch2.hpp>

#include "Common/Dyncast.h"

namespace {

enum class Type {
    Base, LDerivedA, LDerivedB, RDerived, _count
};

struct Base {        
protected:
    explicit Base(Type type): _type(type) {}
    
public:
    Type type() const { return _type; }
    
private:
    Type _type;
};

struct LDerivedA: Base {
protected:
    explicit LDerivedA(Type type): Base(type) {}
    
public:
    LDerivedA(): Base(Type::LDerivedA) {}
};

struct LDerivedB: LDerivedA {
    LDerivedB(): LDerivedA(Type::LDerivedB) {}
};

struct RDerived: Base {
    RDerived(): Base(Type::RDerived) {}
};

} // namespace

SC_DYNCAST_MAP(Base, Type::Base);
SC_DYNCAST_MAP(LDerivedA, Type::LDerivedA);
SC_DYNCAST_MAP(LDerivedB, Type::LDerivedB);
SC_DYNCAST_MAP(RDerived, Type::RDerived);

using namespace scatha;

template <typename To, typename From>
static bool canDyncast(From&& from) {
    return requires { dyncast<To>(from); };
}

TEST_CASE("Dyncast visit", "[common][dyncast]") {
    LDerivedA a;
    Base& base = a;
    CHECK(visit(base, [](Base& base) { return base.type(); }) == Type::LDerivedA);
}

TEST_CASE("Dyncast visit subtree", "[common][dyncast]") {
    LDerivedB b;
    LDerivedA& base = b;
    CHECK(visit(base, [](LDerivedA& a) { return a.type(); }) == Type::LDerivedB);
}

TEST_CASE("isa and dyncast", "[common][dyncast]") {
    LDerivedA la;
    
    CHECK( isa<Base>(la));
    CHECK( isa<LDerivedA>(la));
    CHECK(!isa<LDerivedB>(la));
    CHECK(!isa<RDerived>(la));

    CHECK(dyncast<Base*>(&la)      != nullptr);
    CHECK(dyncast<LDerivedA*>(&la) != nullptr);
    CHECK(dyncast<LDerivedB*>(&la) == nullptr);
    CHECK(!canDyncast<RDerived*>(&la));

    CHECK_NOTHROW(dyncast<Base&>(la));
    CHECK_NOTHROW(dyncast<LDerivedA&>(la));
    CHECK_THROWS( dyncast<LDerivedB&>(la));

    Base const* base = &la;

    CHECK( isa<Base>(*base));
    CHECK( isa<LDerivedA>(*base));
    CHECK(!isa<LDerivedB>(*base));
    CHECK(!isa<RDerived>(*base));

    CHECK(dyncast<Base const*>(base)      != nullptr);
    CHECK(dyncast<LDerivedA const*>(base) != nullptr);
    CHECK(dyncast<LDerivedB const*>(base) == nullptr);
    CHECK(dyncast<RDerived const*>(base)  == nullptr);

    CHECK_NOTHROW(dyncast<Base const&>(*base));
    CHECK_NOTHROW(dyncast<LDerivedA const&>(*base));
    CHECK_THROWS( dyncast<LDerivedB const&>(*base));
    CHECK_THROWS( dyncast<RDerived const&>(*base));

    LDerivedB lb;

    CHECK( isa<Base>(lb));
    CHECK( isa<LDerivedA>(lb));
    CHECK( isa<LDerivedB>(lb));
    CHECK(!isa<RDerived>(lb));

    CHECK(dyncast<Base*>(&lb)      != nullptr);
    CHECK(dyncast<LDerivedA*>(&lb) != nullptr);
    CHECK(dyncast<LDerivedB*>(&lb) != nullptr);
    CHECK(!canDyncast<RDerived*>(&lb));

    CHECK_NOTHROW(dyncast<Base&>(lb));
    CHECK_NOTHROW(dyncast<LDerivedA&>(lb));
    CHECK_NOTHROW(dyncast<LDerivedB&>(lb));

    base = &lb;

    CHECK( isa<Base>(*base));
    CHECK( isa<LDerivedA>(*base));
    CHECK( isa<LDerivedB>(*base));
    CHECK(!isa<RDerived>(*base));

    CHECK(dyncast<Base const*>(base)      != nullptr);
    CHECK(dyncast<LDerivedA const*>(base) != nullptr);
    CHECK(dyncast<LDerivedB const*>(base) != nullptr);
    CHECK(dyncast<RDerived const*>(base)  == nullptr);

    CHECK_NOTHROW(dyncast<Base const&>(*base));
    CHECK_NOTHROW(dyncast<LDerivedA const&>(*base));
    CHECK_NOTHROW(dyncast<LDerivedB const&>(*base));
    CHECK_THROWS( dyncast<RDerived const&>(*base));

    RDerived r;

    CHECK( isa<Base>(r));
    CHECK(!isa<LDerivedA>(r));
    CHECK(!isa<LDerivedB>(r));
    CHECK( isa<RDerived>(r));

    CHECK(dyncast<Base*>(&r) != nullptr);
    CHECK(!canDyncast<LDerivedA*>(&r));
    CHECK(!canDyncast<LDerivedB*>(&r));
    CHECK(dyncast<RDerived*>(&r) != nullptr);

    CHECK_NOTHROW(dyncast<Base&>(r));
    CHECK_NOTHROW(dyncast<RDerived&>(r));

    base = &r;

    CHECK( isa<Base>(*base));
    CHECK(!isa<LDerivedA>(*base));
    CHECK(!isa<LDerivedB>(*base));
    CHECK( isa<RDerived>(*base));

    CHECK( dyncast<Base const*>(base));
    CHECK(!dyncast<LDerivedA const*>(base));
    CHECK(!dyncast<LDerivedB const*>(base));
    CHECK( dyncast<RDerived const*>(base));

    CHECK_NOTHROW(dyncast<Base const&>(*base));
    CHECK_THROWS (dyncast<LDerivedA const&>(*base));
    CHECK_THROWS (dyncast<LDerivedB const&>(*base));
    CHECK_NOTHROW(dyncast<RDerived const&>(*base));
}
