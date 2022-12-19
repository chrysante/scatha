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

SC_DYNCAST_REGISTER_PAIR(Base, Type::Base);
SC_DYNCAST_REGISTER_PAIR(LDerivedA, Type::LDerivedA);
SC_DYNCAST_REGISTER_PAIR(LDerivedB, Type::LDerivedB);
SC_DYNCAST_REGISTER_PAIR(RDerived, Type::RDerived);

using namespace scatha;

template <typename To, typename From>
static bool canDyncast(From&& from) {
    return requires { dyncast<To>(from); };
}

TEST_CASE("DyncastTraits::is", "[common][dyncast]") {
    LDerivedA la;
    
    CHECK( DyncastTraits<Type>::is<Type::Base>(la));
    CHECK( DyncastTraits<Type>::is<Type::LDerivedA>(la));
    CHECK(!DyncastTraits<Type>::is<Type::LDerivedB>(la));
    CHECK(!DyncastTraits<Type>::is<Type::RDerived>(la));
    
    CHECK( dyncast<Base*>(&la));
    CHECK( dyncast<LDerivedA*>(&la));
    CHECK(!dyncast<LDerivedB*>(&la));
    CHECK(!canDyncast<RDerived*>(&la));
    
    CHECK_NOTHROW(dyncast<Base&>(la));
    CHECK_NOTHROW(dyncast<LDerivedA&>(la));
    CHECK_THROWS( dyncast<LDerivedB&>(la));
    
    Base const* base = &la;

    CHECK( DyncastTraits<Type>::is<Type::Base>(*base));
    CHECK( DyncastTraits<Type>::is<Type::LDerivedA>(*base));
    CHECK(!DyncastTraits<Type>::is<Type::LDerivedB>(*base));
    CHECK(!DyncastTraits<Type>::is<Type::RDerived>(*base));

    CHECK( dyncast<Base const*>(base));
    CHECK( dyncast<LDerivedA const*>(base));
    CHECK(!dyncast<LDerivedB const*>(base));
    CHECK(!dyncast<RDerived const*>(base));
    
    CHECK_NOTHROW(dyncast<Base const&>(*base));
    CHECK_NOTHROW(dyncast<LDerivedA const&>(*base));
    CHECK_THROWS( dyncast<LDerivedB const&>(*base));
    CHECK_THROWS( dyncast<RDerived const&>(*base));

    LDerivedB lb;

    CHECK( DyncastTraits<Type>::is<Type::Base>(lb));
    CHECK( DyncastTraits<Type>::is<Type::LDerivedA>(lb));
    CHECK( DyncastTraits<Type>::is<Type::LDerivedB>(lb));
    CHECK(!DyncastTraits<Type>::is<Type::RDerived>(lb));

    CHECK( dyncast<Base*>(&lb));
    CHECK( dyncast<LDerivedA*>(&lb));
    CHECK( dyncast<LDerivedB*>(&lb));
    CHECK(!canDyncast<RDerived*>(&lb));

    CHECK_NOTHROW(dyncast<Base&>(lb));
    CHECK_NOTHROW(dyncast<LDerivedA&>(lb));
    CHECK_NOTHROW(dyncast<LDerivedB&>(lb));
    
    base = &lb;

    CHECK( DyncastTraits<Type>::is<Type::Base>(*base));
    CHECK( DyncastTraits<Type>::is<Type::LDerivedA>(*base));
    CHECK( DyncastTraits<Type>::is<Type::LDerivedB>(*base));
    CHECK(!DyncastTraits<Type>::is<Type::RDerived>(*base));

    CHECK( dyncast<Base const*>(base));
    CHECK( dyncast<LDerivedA const*>(base));
    CHECK( dyncast<LDerivedB const*>(base));
    CHECK(!dyncast<RDerived const*>(base));
    
    CHECK_NOTHROW(dyncast<Base const&>(*base));
    CHECK_NOTHROW(dyncast<LDerivedA const&>(*base));
    CHECK_NOTHROW(dyncast<LDerivedB const&>(*base));
    CHECK_THROWS( dyncast<RDerived const&>(*base));

    RDerived r;

    CHECK( DyncastTraits<Type>::is<Type::Base>(r));
    CHECK(!DyncastTraits<Type>::is<Type::LDerivedA>(r));
    CHECK(!DyncastTraits<Type>::is<Type::LDerivedB>(r));
    CHECK( DyncastTraits<Type>::is<Type::RDerived>(r));

    CHECK( dyncast<Base*>(&r));
    CHECK(!canDyncast<LDerivedA*>(&r));
    CHECK(!canDyncast<LDerivedB*>(&r));
    CHECK( dyncast<RDerived*>(&r));
    
    CHECK_NOTHROW(dyncast<Base&>(r));
    CHECK_NOTHROW(dyncast<RDerived&>(r));

    base = &r;

    CHECK( DyncastTraits<Type>::is<Type::Base>(*base));
    CHECK(!DyncastTraits<Type>::is<Type::LDerivedA>(*base));
    CHECK(!DyncastTraits<Type>::is<Type::LDerivedB>(*base));
    CHECK( DyncastTraits<Type>::is<Type::RDerived>(*base));

    CHECK( dyncast<Base const*>(base));
    CHECK(!dyncast<LDerivedA const*>(base));
    CHECK(!dyncast<LDerivedB const*>(base));
    CHECK( dyncast<RDerived const*>(base));
    
    CHECK_NOTHROW(dyncast<Base const&>(*base));
    CHECK_THROWS (dyncast<LDerivedA const&>(*base));
    CHECK_THROWS (dyncast<LDerivedB const&>(*base));
    CHECK_NOTHROW(dyncast<RDerived const&>(*base));
}
