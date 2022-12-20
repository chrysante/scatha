#ifndef SCATHA_IR_CONTEXT_H_
#define SCATHA_IR_CONTEXT_H_

#include <string>

#include <utl/hashset.hpp>

#include "IR/Type.h"
#include "IR/Value.h"

namespace scatha::ir {

class SCATHA(API) Context {
public:
    Context();
    Type const* voidType();
    Integral const* integralType(std::size_t bitWidth);

private:
    //    utl::hashset<Value> values;
    utl::hashset<Type*, Type::Hash, Type::Equals> types;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CONTEXT_H_
