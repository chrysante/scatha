#include "IR/Context.h"

#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/StructHash.h"
#include "IR/Type.h"

using namespace scatha;

using namespace ir;

using StructKey = utl::small_vector<Type const*>;
using ArrayKey = std::pair<Type const*, size_t>;

struct Context::Impl {
    /// ## Constants
    /// ** Bitwidth must appear before the value, because comparison of values
    /// of different widths may not be possible. **
    utl::hashmap<std::pair<size_t, APInt>, UniquePtr<IntegralConstant>>
        _integralConstants;
    /// We use `std::map` here because floats are not really hashable.
    utl::hashmap<std::pair<size_t, APFloat>, UniquePtr<FloatingPointConstant>>
        _floatConstants;
    utl::hashmap<Type const*, UniquePtr<UndefValue>> _undefConstants;

    /// ## Types
    utl::vector<UniquePtr<Type>> _types;
    VoidType const* _voidType;
    PointerType const* _ptrType;
    utl::hashmap<uint32_t, IntegralType const*> _intTypes;
    utl::hashmap<uint32_t, FloatType const*> _floatTypes;
    utl::hashmap<StructKey,
                 StructType const*,
                 internal::StructHash,
                 internal::StructEqual>
        _anonymousStructs;
    utl::hashmap<ArrayKey, ArrayType const*> _arrayTypes;
};

Context::Context(): impl(std::make_unique<Impl>()) {
    auto vt = allocate<VoidType>();
    impl->_voidType = vt.get();
    impl->_types.push_back(std::move(vt));
    auto pt = allocate<PointerType>();
    impl->_ptrType = pt.get();
    impl->_types.push_back(std::move(pt));
}

Context::Context(Context&&) noexcept = default;

Context& Context::operator=(Context&&) noexcept = default;

Context::~Context() = default;

VoidType const* Context::voidType() { return impl->_voidType; }

PointerType const* Context::ptrType() { return impl->_ptrType; }

template <typename A>
static auto* getArithmeticType(size_t bitwidth, auto& types, auto& map) {
    auto itr = map.find(bitwidth);
    if (itr != map.end()) {
        return itr->second;
    }
    auto type = allocate<A>(bitwidth);
    itr = map.insert({ bitwidth, type.get() }).first;
    types.push_back(std::move(type));
    return itr->second;
}

IntegralType const* Context::intType(size_t bitwidth) {
    return getArithmeticType<IntegralType>(bitwidth,
                                           impl->_types,
                                           impl->_intTypes);
}

IntegralType const* Context::boolType() { return intType(1); }

FloatType const* Context::floatType(size_t bitwidth) {
    SC_ASSERT(bitwidth == 32 || bitwidth == 64, "Other sizes not supported");
    return getArithmeticType<FloatType>(bitwidth,
                                        impl->_types,
                                        impl->_floatTypes);
}

FloatType const* Context::floatType(APFloatPrec precision) {
    return floatType(precision.totalBitwidth());
}

StructType const* Context::anonymousStruct(
    std::span<Type const* const> members) {
    auto itr = impl->_anonymousStructs.find(members);
    if (itr != impl->_anonymousStructs.end()) {
        return itr->second;
    }
    auto type = allocate<StructType>(std::string{});
    for (auto* member: members) {
        type->pushMember(member);
    }
    auto* result = type.get();
    impl->_types.push_back(std::move(type));
    impl->_anonymousStructs.insert({ members | ranges::to<StructKey>, result });
    return result;
}

ArrayType const* Context::arrayType(Type const* elementType, size_t count) {
    ArrayKey key = { elementType, count };
    auto itr = impl->_arrayTypes.find(key);
    if (itr != impl->_arrayTypes.end()) {
        return itr->second;
    }
    auto type = allocate<ArrayType>(elementType, count);
    itr = impl->_arrayTypes.insert({ key, type.get() }).first;
    impl->_types.push_back(std::move(type));
    return itr->second;
}

ArrayType const* Context::byteArrayType(size_t count) {
    return arrayType(intType(8), count);
}

IntegralConstant* Context::intConstant(APInt value) {
    size_t const bitwidth = value.bitwidth();
    auto itr = impl->_integralConstants.find({ bitwidth, value });
    if (itr == impl->_integralConstants.end()) {
        std::tie(itr, std::ignore) = impl->_integralConstants.insert(
            { std::pair{ bitwidth, value },
              allocate<IntegralConstant>(*this, value) });
    }
    SC_ASSERT(ucmp(itr->second->value(), value) == 0, "Value mismatch");
    return itr->second.get();
}

IntegralConstant* Context::intConstant(u64 value, size_t bitwidth) {
    return intConstant(APInt(value, bitwidth));
}

IntegralConstant* Context::boolConstant(bool value) {
    return intConstant(value, 1);
}

FloatingPointConstant* Context::floatConstant(APFloat value) {
    size_t const bitwidth = value.precision().totalBitwidth();
    auto itr = impl->_floatConstants.find({ bitwidth, value });
    if (itr == impl->_floatConstants.end()) {
        std::tie(itr, std::ignore) = impl->_floatConstants.insert(
            { std::pair{ bitwidth, value },
              allocate<FloatingPointConstant>(*this, value) });
    }
    SC_ASSERT(itr->second->value() == value, "Value mismatch");
    return itr->second.get();
}

FloatingPointConstant* Context::floatConstant(double value, size_t bitwidth) {
    switch (bitwidth) {
    case 32:
        return floatConstant(APFloat(value, APFloatPrec::Single));
    case 64:
        return floatConstant(APFloat(value, APFloatPrec::Double));
    default:
        SC_UNREACHABLE();
    }
}

Constant* Context::arithmeticConstant(int64_t value, Type const* type) {
    // clang-format off
    return visit(*type, utl::overload{
        [&](IntegralType const& type) {
            return intConstant(static_cast<uint64_t>(value),
                                    type.bitwidth());
        },
        [&](FloatType const& type) {
            return floatConstant(value, type.bitwidth());
        },
        [&](Type const& type) -> Constant* { SC_UNREACHABLE(); }
    }); // clang-format on
}

Constant* Context::arithmeticConstant(APInt value) {
    return intConstant(value);
}

Constant* Context::arithmeticConstant(APFloat value) {
    return floatConstant(value);
}

UndefValue* Context::undef(Type const* type) {
    auto itr = impl->_undefConstants.find(type);
    if (itr == impl->_undefConstants.end()) {
        bool success = false;
        std::tie(itr, success) =
            impl->_undefConstants.insert({ type, allocate<UndefValue>(type) });
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
