#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <string>

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>

#include "Common/APInt.h"
#include "IR/Type.h"
#include "IR/Value.h"


namespace scatha::ir {

class SCATHA(API) Context {
public:
    Context();
    
    Type const* voidType();
    
    Integral const* integralType(size_t bitWidth);
    
    Type const* pointerType();
    
    IntegralConstant* getIntegralConstant(APInt value, size_t bitWidth);
    
    void addGlobal(Constant* constant);
    
    Constant* getGlobal(std::string_view name) const;
    
private:
    utl::hashmap<std::pair<APInt, size_t>, IntegralConstant*, utl::hash<std::pair<APInt, size_t>>> _integralConstants;
    utl::hashset<Type*, Type::Hash, Type::Equals> _types;
    utl::hashmap<std::string, Constant*, utl::hash<std::string>, std::equal_to<>> _globals;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_
