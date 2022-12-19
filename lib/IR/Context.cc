#include "IR/Context.h"

#include <utl/strcat.hpp>

using namespace scatha;

using namespace ir;

Context::Context() {
    types.insert(new Type("void", Type::Category::Void));
}

Type const* Context::voidType() {
    auto itr = types.find("void");
    SC_ASSERT(itr != types.end(), "");
    return *itr;
}

Integral const* Context::integralType(std::size_t bitWidth) {
    std::string const name = utl::strcat("i", bitWidth);
    auto itr = types.find(name);
    if (itr == types.end()) {
        types.insert(new Integral(bitWidth));
    }
    SC_ASSERT((*itr)->category() == Type::Integral, "");
    return static_cast<Integral const*>(*itr);
}
