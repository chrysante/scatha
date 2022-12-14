// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <map> // For now, we shouldn't actually use this.
#include <string>

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>

#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/IR/CFGCommon.h>
#include <scatha/IR/Type.h>

namespace scatha::ir {

class SCATHA(API) Context {
public:
    Context();

    Type const* voidType();

    Type const* pointerType();

    Integral const* integralType(size_t bitWidth);

    FloatingPoint const* floatType(size_t bitWidth);

    IntegralConstant* integralConstant(APInt value, size_t bitWidth);

    FloatingPointConstant* floatConstant(APFloat value, size_t bitWidth);

    void addGlobal(Constant* constant);

    Constant* getGlobal(std::string_view name) const;

private:
    utl::hashmap<std::pair<APInt, size_t>, IntegralConstant*> _integralConstants;
    std::map<std::pair<APFloat, size_t>, FloatingPointConstant*> _floatConstants;
    utl::hashset<Type*, Type::Hash, Type::Equals> _types;
    utl::hashmap<std::string, Constant*> _globals;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_
