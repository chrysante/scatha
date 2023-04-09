#include "IR/CFG/Constants.h"

#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

IntegralConstant::IntegralConstant(Context& context,
                                   APInt value,
                                   size_t bitWidth):
    Constant(NodeType::IntegralConstant, context.integralType(bitWidth)),
    _value(value) {}

IntegralType const* IntegralConstant::type() const {
    return cast<IntegralType const*>(Value::type());
}

FloatingPointConstant::FloatingPointConstant(Context& context,
                                             APFloat value,
                                             size_t bitWidth):
    Constant(NodeType::FloatingPointConstant, context.floatType(bitWidth)),
    _value(value) {}

FloatType const* FloatingPointConstant::type() const {
    return cast<FloatType const*>(Value::type());
}
