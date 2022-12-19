#ifndef SCATHA_IR_FUNCTION_H_
#define SCATHA_IR_FUNCTION_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Value.h"
#include "IR/Type.h"

namespace scatha::ir {

class Function: public Constant {
public:
    explicit Function(FunctionType* functionType, std::span<Type const*> parameterTypes):
        Constant(functionType), _parameterTypes(std::move(parameterTypes))
    {}
    
private:
    utl::small_vector<Type const*> _parameterTypes;
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_FUNCTION_H_

