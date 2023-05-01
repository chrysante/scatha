#include "IR/Context.h"

#include <utl/strcat.hpp>

#include "IR/CFG.h"
#include "IR/Type.h"

using namespace scatha;

using namespace ir;

Context::Context() {
    auto vt   = allocate<VoidType>();
    _voidType = vt.get();
    _types.push_back(std::move(vt));
    auto pt  = allocate<PointerType>();
    _ptrType = pt.get();
    _types.push_back(std::move(pt));
}

Context::Context(Context&&) noexcept = default;

Context& Context::operator=(Context&&) noexcept = default;

Context::~Context() = default;

template <typename A>
static auto* getArithmeticType(size_t bitWidth, auto& types, auto& map) {
    auto itr = map.find(bitWidth);
    if (itr != map.end()) {
        return itr->second;
    }
    auto type = allocate<A>(bitWidth);
    itr       = map.insert({ bitWidth, type.get() }).first;
    types.push_back(std::move(type));
    return itr->second;
}

IntegralType const* Context::integralType(size_t bitWidth) {
    return getArithmeticType<IntegralType>(bitWidth, _types, _intTypes);
}

FloatType const* Context::floatType(size_t bitWidth) {
    SC_ASSERT(bitWidth == 32 || bitWidth == 64, "Other sizes not supported");
    return getArithmeticType<FloatType>(bitWidth, _types, _floatTypes);
}

/// \returns The array type of \p elementType with \p count elements
ArrayType const* Context::arrayType(Type const* elementType, size_t count) {
    ArrayKey key = { elementType, count };
    auto itr     = _arrayTypes.find(key);
    if (itr != _arrayTypes.end()) {
        return itr->second;
    }
    auto type = allocate<ArrayType>(elementType, count);
    itr       = _arrayTypes.insert({ key, type.get() }).first;
    _types.push_back(std::move(type));
    return itr->second;
}

IntegralConstant* Context::integralConstant(APInt value) {
    size_t const bitwidth = value.bitwidth();
    auto itr              = _integralConstants.find({ bitwidth, value });
    if (itr == _integralConstants.end()) {
        std::tie(itr, std::ignore) = _integralConstants.insert(
            { std::pair{ bitwidth, value },
              allocate<IntegralConstant>(*this, value, bitwidth) });
    }
    SC_ASSERT(ucmp(itr->second->value(), value) == 0, "Value mismatch");
    return itr->second.get();
}

IntegralConstant* Context::integralConstant(u64 value, size_t bitWidth) {
    return integralConstant(APInt(value, bitWidth));
}

FloatingPointConstant* Context::floatConstant(APFloat value, size_t bitWidth) {
    auto itr = _floatConstants.find({ bitWidth, value });
    if (itr == _floatConstants.end()) {
        std::tie(itr, std::ignore) = _floatConstants.insert(
            { std::pair{ bitWidth, value },
              allocate<FloatingPointConstant>(*this, value, bitWidth) });
    }
    SC_ASSERT(itr->second->value() == value, "Value mismatch");
    return itr->second.get();
}

FloatingPointConstant* Context::floatConstant(double value, size_t bitWidth) {
    switch (bitWidth) {
    case 32:
        return floatConstant(APFloat(value, APFloatPrec::Single), 32);
    case 64:
        return floatConstant(APFloat(value, APFloatPrec::Double), 64);
    default:
        SC_UNREACHABLE();
    }
}

Constant* Context::arithmeticConstant(int64_t value, Type const* type) {
    // clang-format off
    return visit(*type, utl::overload{
        [&](IntegralType const& type) {
            return integralConstant(static_cast<uint64_t>(value),
                                    type.bitWidth());
        },
        [&](FloatType const& type) {
            return floatConstant(value, type.bitWidth());
        },
        [&](Type const& type) -> Constant* { SC_UNREACHABLE(); }
    }); // clang-format on
}

Constant* Context::arithmeticConstant(APInt value) {
    return integralConstant(value);
}

Constant* Context::arithmeticConstant(APFloat value) {
    size_t const bitwidth = value.precision() == APFloatPrec::Single ? 32 : 64;
    return floatConstant(value, bitwidth);
}

UndefValue* Context::undef(Type const* type) {
    auto itr = _undefConstants.find(type);
    if (itr == _undefConstants.end()) {
        bool success = false;
        std::tie(itr, success) =
            _undefConstants.insert({ type, allocate<UndefValue>(type) });
        SC_ASSERT(success, "");
    }
    return itr->second.get();
}

Value* Context::voidValue() { return undef(voidType()); }

bool Context::isCommutative(ArithmeticOperation operation) const {
    switch (operation) {
    case ArithmeticOperation::Add:
    case ArithmeticOperation::Mul:
    case ArithmeticOperation::And:
    case ArithmeticOperation::Or:
    case ArithmeticOperation::XOr:
    case ArithmeticOperation::FAdd:
    case ArithmeticOperation::FMul:
        return true;

    default:
        return false;
    }
}

bool Context::isAssociative(ArithmeticOperation operation) const {
    switch (operation) {
    case ArithmeticOperation::Add:
    case ArithmeticOperation::Mul:
    case ArithmeticOperation::And:
    case ArithmeticOperation::Or:
    case ArithmeticOperation::XOr:
        return true;

    case ArithmeticOperation::FAdd:
    case ArithmeticOperation::FMul:
        return associativeFloatArithmetic();

    default:
        return false;
    }
}
