#include "IR/Context.h"

#include <utl/strcat.hpp>

using namespace scatha;

using namespace ir;

Context::Context() {
    _types.insert(new Type("void", Type::Category::Void));
    _types.insert(new Type("ptr", Type::Category::Pointer));
}

Type const* Context::voidType() {
    auto itr = _types.find("void");
    SC_ASSERT(itr != _types.end(), "Void must exist");
    return *itr;
}

Integral const* Context::integralType(size_t bitWidth) {
    std::string const name = utl::strcat("i", bitWidth);
    auto itr = _types.find(name);
    if (itr == _types.end()) {
        std::tie(itr, std::ignore) = _types.insert(new Integral(bitWidth));
    }
    SC_ASSERT((*itr)->category() == Type::Integral, "Category of Integral must be Integral");
    return static_cast<Integral const*>(*itr);
}

Type const* Context::pointerType() {
    auto itr = _types.find("ptr");
    SC_ASSERT(itr != _types.end(), "Ptr type must exist");
    return *itr;
}

IntegralConstant* Context::getIntegralConstant(APInt value, size_t bitWidth) {
    auto itr = _integralConstants.find({ value, bitWidth });
    if (itr == _integralConstants.end()) {
        std::tie(itr, std::ignore) = _integralConstants.insert({ { value, bitWidth }, new IntegralConstant(*this, value, bitWidth) });
    }
    SC_ASSERT(itr->second->value() == value, "Value mismatch");
    return itr->second;
}

void Context::addGlobal(Constant* constant) {
    auto const [_, success] = _globals.insert({ std::string(constant->name()), constant });
    SC_ASSERT(success, "Name already in use?");
}

Constant* Context::getGlobal(std::string_view name) const {
    auto itr = _globals.find(name);
    SC_ASSERT(itr != _globals.end(), "Undeclared name");
    return itr->second;
}
