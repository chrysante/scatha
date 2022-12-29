#include "APFloatTest.h"

#include <iomanip>
#include <iostream>

#include "Common/APFloat.h"

using namespace scatha;

[[maybe_unused]] static void printAPFloat(std::string_view name, APFloat const& f) {
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

void playground::apFloatTest() {

}
