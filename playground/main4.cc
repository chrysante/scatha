#include <iostream>

#include "Common/APFloat.h"

using namespace scatha;

void printInfo(std::string_view name, APFloat const& f) {
    std::cout << name << " = " << f << std::endl;
    std::cout << "Exponent: " << f.exponent() << std::endl;
    std::cout << "Mantissa limbs: " << f.mantissa().size() << ": " << std::endl;
    for (auto i: f.mantissa()) {
        std::cout << std::bitset<sizeof(i) * CHAR_BIT>(i) << "\n";
    }
}

std::optional<long> stringToLong(std::string const& s) {
    try {
        return std::stol(s);
    }
    catch (std::out_of_range const&) {
        return std::nullopt;
    }
}

int main() {

    APFloat const f = APFloat::parse(".3").value();
    APFloat const g =                 .3;
    
    printInfo("f", f);
    printInfo("g", g);

    std::cout << "f " << (f == g ? "==" : "!=") << " g" << std::endl;
    
    
    long x = stringToLong("1214543676875645342312456789865432567897768574325678").value_or(-1);
    
    std::cout << "x = " << x << std::endl;
}
