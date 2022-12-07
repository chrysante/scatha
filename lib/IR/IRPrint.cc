#include "IRPrint.h"

#include <iostream>

#include "IR/Program.h"

using namespace scatha;
using namespace ir;

void ir::print(Program const& program, SymbolTable const& symbolTable) {
    ir::print(program, symbolTable, std::cout);
}

void ir::print(Program const& program, SymbolTable const& symbolTable, std::ostream& str) {
    for (auto& structure: program.structures()) {
//        str << structure
    }
}
