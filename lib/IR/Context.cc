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

IntegralConstant* Context::integralConstant(APInt value) {
    size_t const bitwidth = value.bitwidth();
    auto itr              = _integralConstants.find({ value, bitwidth });
    if (itr == _integralConstants.end()) {
        std::tie(itr, std::ignore) = _integralConstants.insert(
            { { value, bitwidth },
              new IntegralConstant(*this, value, bitwidth) });
    }
    SC_ASSERT(ucmp(itr->second->value(), value) == 0, "Value mismatch");
    return itr->second;
}

IntegralConstant* Context::integralConstant(u64 value, size_t bitWidth) {
    return integralConstant(APInt(value, bitWidth));
}

FloatingPointConstant* Context::floatConstant(APFloat value, size_t bitWidth) {
    auto itr = _floatConstants.find({ value, bitWidth });
    if (itr == _floatConstants.end()) {
        std::tie(itr, std::ignore) = _floatConstants.insert(
            { { value, bitWidth },
              new FloatingPointConstant(*this, value, bitWidth) });
    }
    SC_ASSERT(itr->second->value() == value, "Value mismatch");
    return itr->second;
}

UndefValue* Context::undef(Type const* type) {
    auto itr = _undefConstants.find(type);
    if (itr == _undefConstants.end()) {
        bool success = false;
        std::tie(itr, success) =
            _undefConstants.insert({ type, new UndefValue(type) });
        SC_ASSERT(success, "");
    }
    return itr->second;
}

Value* Context::voidValue() {
    return undef(voidType());
}

std::string Context::uniqueName(Function const* function, std::string name) {
    auto const [itr, success] = varIndices.insert({ { function, name }, 0 });
    if (success) {
        return name;
    }
    return utl::strcat(name, ".", ++itr->second);
}
