#include <iostream>

#include <utl/typeinfo.hpp>

#include "IR/BasicBlock.h"
#include "IR/Terminators.h"

#include "Common/Dyncast.h"

using namespace scatha;

enum class Type {
   Base, LDerivedA, LDerivedB, RDerived, _count
};

struct Base;
struct LDerivedA;
struct LDerivedB;
struct RDerived;

SC_DYNCAST_REGISTER_PAIR(Base, Type::Base);
SC_DYNCAST_REGISTER_PAIR(LDerivedA, Type::LDerivedA);
SC_DYNCAST_REGISTER_PAIR(LDerivedB, Type::LDerivedB);
SC_DYNCAST_REGISTER_PAIR(RDerived, Type::RDerived);

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

int main() {
 
    
    RDerived value;
    
    Base& base = value;
    
    
    scatha::visit(base, []<typename T>(T&&) {
        std::cout << utl::nameof<T> << std::endl;
    });
    
    std::cout << std::boolalpha;
    
    std::cout << DyncastTraits<Type>::is<Type::Base>(base) << std::endl << std::endl;
    std::cout << DyncastTraits<Type>::is<Type::LDerivedA>(base) << std::endl << std::endl;
    std::cout << DyncastTraits<Type>::is<Type::LDerivedB>(base) << std::endl << std::endl;
    std::cout << DyncastTraits<Type>::is<Type::RDerived>(base) << std::endl << std::endl;
    
    return 0;
    ir::Context theContext;
    
    auto* bb = new ir::BasicBlock(theContext, "block");
    
    std::cout << bb->type()->name() << std::endl;
    
    
    
}
