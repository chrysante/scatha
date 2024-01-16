#include "IRGen/CallingConvention.h"

#include <iostream>

#include <range/v3/view.hpp>

using namespace scatha;
using namespace irgen;

std::ostream& irgen::operator<<(std::ostream& str, PassingConvention PC) {
    return str << "[" << PC.location() << ", " << PC.numParams() << "]";
}

void irgen::print(CallingConvention const& CC) { print(CC, std::cout); }

void irgen::print(CallingConvention const& CC, std::ostream& str) {
    str << "ReturnValue: " << CC.returnValue() << std::endl;
    str << "Parameters:  " << CC.argument(0) << std::endl;
    for (auto& PC: CC.arguments() | ranges::views::drop(1)) {
        str << "             " << PC << std::endl;
    }
}
