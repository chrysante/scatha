#include "IR/CFG/Constants.h"

#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

IntegralConstant::IntegralConstant(Context& context, APInt value):
    Constant(NodeType::IntegralConstant, context.intType(value.bitwidth())),
    _value(value) {}

IntegralType const* IntegralConstant::type() const {
    return cast<IntegralType const*>(Value::type());
}

FloatingPointConstant::FloatingPointConstant(Context& context, APFloat value):
    Constant(NodeType::FloatingPointConstant,
             context.floatType(value.precision())),
    _value(value) {}

FloatType const* FloatingPointConstant::type() const {
    return cast<FloatType const*>(Value::type());
}

ConstantData::ConstantData(ir::Context& ctx,
                           Type const* dataType,
                           utl::vector<uint8_t> data,
                           std::string name):
    Constant(NodeType::ConstantData, ctx.ptrType(), std::move(name)),
    _dataType(dataType),
    _data(std::move(data)) {}
