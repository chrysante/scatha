#include "IR/Value.h"

#include "IR/Context.h"

using namespace scatha;
using namespace ir;

IntegralConstant::IntegralConstant(Context& context, APInt value, size_t bitWidth):
    Constant(NodeType::IntegralConstant, context.integralType(bitWidth)), _value(value) {}
