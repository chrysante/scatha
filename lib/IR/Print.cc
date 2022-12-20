#include "IR/Print.h"

#include <iostream>

#include "IR/Module.h"

using namespace scatha;
using namespace ir;

void ir::print(Module const& program) {
    ir::print(program, std::cout);
}

void ir::print(Module const& program, std::ostream& str) {
//    for (auto& structure: program.structures()) {
//        str << structure
//    }
}
