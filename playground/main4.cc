#include <iostream>
#include <iomanip>

#include "Common/APFloat.h"

#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Function.h"
#include "IR/Instruction.h"

using namespace scatha;

static void printAPFloat(std::string_view name, APFloat const& f) {
    std::cout << name << ":\n";
    std::cout << "\tExponent:       " << f.exponent() << "\n";
    std::cout << "\tPrecision:      " << f.precision() << "\n";
    std::cout << "\tMantissa limbs: " << f.mantissa().size() << "\n";
    if (!f.mantissa().empty()) {
        std::cout << "\tMantissa:       ";
        for (bool first = true; auto i: f.mantissa()) {
            if (!first) { std::cout << "\t                "; } else { first = false; }
            std::cout << std::bitset<64>(i) << std::endl;
        }
    }
    std::cout << "\tValue:          " << f << "\n";
}

int main() {
    
//    ir::Context ctx;
//
//    auto* entry = new ir::BasicBlock(ctx, "entry");
//    auto* loopHeader = new ir::BasicBlock(ctx, "loop_header");
//
//
//
//    auto* loopHeader = new ir::Branch(ctx, <#Value *condition#>, <#BasicBlock *ifTarget#>, <#BasicBlock *elseTarget#>)(ctx, "loop_header");
    
}
