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
static auto* getArithmeticType(size_t bitwidth, auto& types, auto& map) {
    auto itr = map.find(bitwidth);
    if (itr != map.end()) {
        return itr->second;
    }
    auto type = allocate<A>(bitwidth);
    itr       = map.insert({ bitwidth, type.get() }).first;
    types.push_back(std::move(type));
    return itr->second;
}

IntegralType const* Context::integralType(size_t bitwidth) {
    return getArithmeticType<IntegralType>(bitwidth, _types, _intTypes);
}

FloatType const* Context::floatType(size_t bitwidth) {
    SC_ASSERT(bitwidth == 32 || bitwidth == 64, "Other sizes not supported");
    return getArithmeticType<FloatType>(bitwidth, _types, _floatTypes);
}

StructureType const* Context::anonymousStructure(
    std::span<Type const* const> members) {
    auto itr = _anonymousStructs.find(members);
    if (itr != _anonymousStructs.end()) {
        return itr->second;
    }
    auto type = allocate<StructureType>(std::string{});
    for (auto* member: members) {
        type->addMember(member);
    }
    auto* result = type.get();
    _types.push_back(std::move(type));
    _anonymousStructs.insert({ members | ranges::to<StructKey>, result });
    return result;
}

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

IntegralConstant* Context::integralConstant(u64 value, size_t bitwidth) {
    return integralConstant(APInt(value, bitwidth));
}

FloatingPointConstant* Context::floatConstant(APFloat value, size_t bitwidth) {
    auto itr = _floatConstants.find({ bitwidth, value });
    if (itr == _floatConstants.end()) {
        std::tie(itr, std::ignore) = _floatConstants.insert(
            { std::pair{ bitwidth, value },
              allocate<FloatingPointConstant>(*this, value, bitwidth) });
    }
    SC_ASSERT(itr->second->value() == value, "Value mismatch");
    return itr->second.get();
}

FloatingPointConstant* Context::floatConstant(double value, size_t bitwidth) {
    switch (bitwidth) {
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
                                    type.bitwidth());
        },
        [&](FloatType const& type) {
            return floatConstant(value, type.bitwidth());
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
