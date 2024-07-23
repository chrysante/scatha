#include <iostream>

#include "IR/CFG.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"

using namespace scatha;
using namespace ir;

static bool helloWorld(Context&, Function& F, PassArgumentMap const& args) {
    auto message = args.get<std::string>("message");
    bool printFunction = args.get<bool>("print");
    std::cout << message << "! The name of the function is: " << F.name()
              << std::endl;
    if (printFunction) {
        print(F);
    }
    return false;
}

using namespace passParameterTypes;

SC_REGISTER_PASS(helloWorld, "helloworld", ir::PassCategory::Other,
                 { String{ "message", "Hello World" },
                   Flag{ "print", false } });
