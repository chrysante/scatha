#include "IRGen/CallingConvention.h"

#include <iostream>

#include <range/v3/view.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

utl::small_vector<ValueLocation, 2> PassingConvention::locationsAtCallsite()
    const {
    auto locs = locations() | ToSmallVector<2>;
    SC_EXPECT(!locs.empty());
    if (isa<sema::ReferenceType>(type())) {
        locs[0] = ValueLocation::Memory;
    }
    return locs;
}

std::ostream& irgen::operator<<(std::ostream& str, PassingConvention PC) {
    str << "[";
    for (bool first = true; auto loc: PC.locations()) {
        if (!first) {
            str << ", ";
        }
        first = false;
        str << loc;
    }
    return str << "]";
}

void irgen::print(CallingConvention const& CC) { print(CC, std::cout); }

void irgen::print(CallingConvention const& CC, std::ostream& str) {
    str << "Return value: " << CC.returnLocation() << "\n";
    str << "Parameters:  " << CC.argument(0) << "\n";
    for (auto& PC: CC.arguments() | ranges::views::drop(1)) {
        str << "             " << PC << "\n";
    }
}

ValueLocation CallingConvention::returnLocationAtCallsite() const {
    if (isa<sema::ReferenceType>(returnType())) {
        return ValueLocation::Memory;
    }
    else {
        return returnLocation();
    }
}
