#include "IR/Context.h"

#include <utl/strcat.hpp>

#include "IR/CFG.h"

using namespace scatha;

using namespace ir;

Context::Context() {
    _types.insert(new VoidType());
}

VoidType const* Context::voidType() {
    auto itr = _types.find("void");
    SC_ASSERT(itr != _types.end(), "Void must exist");
    return cast<VoidType const*>(*itr);
}

PointerType const* Context::pointerType() {
    auto itr = _types.find("ptr");
    if (itr != _types.end()) {
        return cast<PointerType const*>(*itr);
    }
    auto* type = new PointerType();
    _types.insert(type);
    return type;
}

IntegralType const* Context::integralType(size_t bitWidth) {
    auto itr = _types.find(utl::strcat("i", bitWidth));
    if (itr != _types.end()) {
        return cast<IntegralType const*>(*itr);
    }
    auto* type = new IntegralType(bitWidth);
    _types.insert(type);
    return type;
}

FloatType const* Context::floatType(size_t bitWidth) {
    auto itr = _types.find(utl::strcat("f", bitWidth));
    if (itr != _types.end()) {
        return cast<FloatType const*>(*itr);
    }
    auto* type = new FloatType(bitWidth);
    _types.insert(type);
    return type;
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
        std::tie(itr, success) = _undefConstants.insert({ type, new UndefValue(type) });
        SC_ASSERT(success, "");
    }
    return itr->second;
}

void Context::addGlobal(Constant* constant) {
    auto const [_, success] =
        _globals.insert({ std::string(constant->name()), constant });
    SC_ASSERT(success, "Name already in use?");
}

Constant* Context::getGlobal(std::string_view name) const {
    auto itr = _globals.find(name);
    SC_ASSERT(itr != _globals.end(), "Undeclared name");
    return itr->second;
}

std::string Context::uniqueName(Function const* function, std::string name) {
    auto const [itr, success] = varIndices.insert({ { function, name }, 0 });
    if (success) {
        return name;
    }
    return utl::strcat(name, ".", ++itr->second);
}
