#include <iostream>
#include <iomanip>

#include "Common/APFloat.h"

using namespace scatha;

static void printAPFloat(std::string_view name, APFloat const& f) {
    std::cout << name << ":\n";
    std::cout << "\tExponent:      " << f.exponent() << "\n";
    std::cout << "\tPrecision:     " << static_cast<int>(f.precision()) << "\n";
    std::cout << "\tMantissa size: " << f.mantissa().size() << "\n";
    if (!f.mantissa().empty()) {
        std::cout << "\tMantissa:      ";
        for (bool first = true; auto i: f.mantissa()) {
            if (!first) { std::cout << "\t               "; } else { first = false; }
            std::cout << std::bitset<64>(i) << std::endl;
        }
    }
    std::cout << "\tValue:         " << f << "\n";
}

int main() {
    
    double const base = std::numeric_limits<double>::max();
    
    std::cout << std::setprecision(100) << base << std::endl;
    
    APFloat f = APFloat::parse("10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                                "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000").value();
    
    double d = 10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.;
    
    std::cout << d << "\n";
    
    std::cout << (d == base) << std::endl;
    
//    printAPFloat("f", f);
    
    
    
}
