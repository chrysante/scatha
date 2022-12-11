#include "IR/IRPrint.h"

#include <iostream>

#include "IR/Module.h"

using namespace scatha;
using namespace ir;

void ir::print(Module const& program, SymbolTable const& symbolTable) {
    ir::print(program, symbolTable, std::cout);
}

void ir::print(Module const& program, SymbolTable const& symbolTable, std::ostream& str) {
//    for (auto& structure: program.structures()) {
//        str << structure
//    }
}
