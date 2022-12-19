#include <iostream>
#include <iomanip>

#include "Common/APFloat.h"

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
    
    double const base = std::numeric_limits<double>::max();
    
    std::cout << base << std::endl;
    
    APFloat f = APFloat::parse("0.0").value();

    std::cout << f.isInf() << std::endl;
    
    std::cout << "Single::maxExponent(): " << APFloat::Precision::Single.maxExponent() << std::endl;
    std::cout << "Single::minExponent(): " << APFloat::Precision::Single.minExponent() << std::endl;
    
    std::cout << "Double::maxExponent(): " << APFloat::Precision::Double.maxExponent() << std::endl;
    std::cout << "Double::minExponent(): " << APFloat::Precision::Double.minExponent() << std::endl;
    
    
    
    printAPFloat("f", f);
    
}
