#include "IR/CFG/Constants.h"

#include <range/v3/view.hpp>

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

void IntegralConstant::writeValueToImpl(void* dest) const {
    std::memcpy(dest, value().limbs().data(), type()->bytewidth());
}

FloatingPointConstant::FloatingPointConstant(Context& context, APFloat value):
    Constant(NodeType::FloatingPointConstant,
             context.floatType(value.precision())),
    _value(value) {}

FloatType const* FloatingPointConstant::type() const {
    return cast<FloatType const*>(Value::type());
}

void FloatingPointConstant::writeValueToImpl(void* dest) const {
    std::memcpy(dest, value().limbs().data(), type()->bytewidth());
}

NullPointerConstant::NullPointerConstant(PointerType const* ptrType):
    Constant(NodeType::NullPointerConstant, ptrType) {}

PointerType const* NullPointerConstant::type() const {
    return cast<PointerType const*>(Value::type());
}

void NullPointerConstant::writeValueToImpl(void* dest) const {
    std::memset(dest, 0, 8);
}

RecordType const* RecordConstant::type() const {
    return cast<RecordType const*>(Value::type());
}

RecordConstant::RecordConstant(NodeType nodeType,
                               std::span<ir::Constant* const> elems,
                               RecordType const* type):
    Constant(nodeType,
             type,
             {},
             elems | ranges::views::transform(cast<Value*>) | ToSmallVector<>) {
    SC_ASSERT(elems.size() == type->numElements(), "Element count mismatch");
    for (auto [index, elem]: elems | ranges::views::enumerate) {
        SC_ASSERT(elem->type() == type->elementAt(index), "Type mismatch");
    }
}

static void* advance(void* ptr, size_t offset) {
    return static_cast<char*>(ptr) + offset;
}

void RecordConstant::writeValueToImpl(void* dest) const {
    for (auto [value, offset]:
         ranges::views::zip(elements(), type()->offsets()))
    {
        value->writeValueTo(advance(dest, offset));
    }
}

StructConstant::StructConstant(std::span<ir::Constant* const> elems,
                               StructType const* type):
    RecordConstant(NodeType::StructConstant, elems, type) {}

StructType const* StructConstant::type() const {
    return cast<StructType const*>(Value::type());
}

ArrayConstant::ArrayConstant(std::span<ir::Constant* const> elems,
                             ArrayType const* type):
    RecordConstant(NodeType::ArrayConstant, elems, type) {}

ArrayType const* ArrayConstant::type() const {
    return cast<ArrayType const*>(Value::type());
}
