#include "Common/APFwd.h"

#include <ostream>

using namespace scatha;

APFloatPrecision const APFloatPrecision::Single(23, 8);

APFloatPrecision const APFloatPrecision::Double(52, 11);

APFloatPrecision const APFloatPrecision::Quadruple(112, 15);

APFloatPrecision const APFloatPrecision::Default = APFloatPrecision::Double;

std::ostream& scatha::operator<<(std::ostream& ostream, APFloatPrecision precision) {
    return ostream << "{ .mantissaBits = " << precision.mantissaBits() << ", .exponentBits = "
                   << precision.exponentBits() << " }";
}
