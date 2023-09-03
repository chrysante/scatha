#include "AST/Lowering/CallingConvention.h"

#include <iostream>

#include <range/v3/view.hpp>

using namespace scatha;
using namespace ast;

std::ostream& ast::operator<<(std::ostream& str, PassingConvention PC) {
    return str << "[" << PC.location() << ", " << PC.numParams() << "]";
}

void ast::print(CallingConvention const& CC) { print(CC, std::cout); }

void ast::print(CallingConvention const& CC, std::ostream& str) {
    str << "ReturnValue: " << CC.returnValue() << std::endl;
    str << "Parameters:  " << CC.argument(0) << std::endl;
    for (auto& PC: CC.arguments() | ranges::views::drop(1)) {
        str << "             " << PC << std::endl;
    }
}
