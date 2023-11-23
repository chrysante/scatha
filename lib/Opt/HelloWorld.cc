#include <iostream>

#include "IR/CFG.h"
#include "IR/PassRegistry.h"

using namespace scatha;

static bool helloWorld(ir::Context&, ir::Function& F) {
    std::cout << "Hello World! The name of the function is: " << F.name()
              << std::endl;
    return false;
}

SC_REGISTER_PASS(helloWorld, "helloworld");
