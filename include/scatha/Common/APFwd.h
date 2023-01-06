// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_APFWD_H_
#define SCATHA_COMMON_APFWD_H_

#include <iosfwd>

#include <scatha/Basic/Basic.h>

namespace scatha {

class APInt;
class APFloat;

class SCATHA(API) APFloatPrecision {
public:
    static const APFloatPrecision Single;
    static const APFloatPrecision Double;
    static const APFloatPrecision Quadruple;
    static const APFloatPrecision Default;

    explicit APFloatPrecision(int mantissaBits, int exponentBits):
        _mantissaBits(mantissaBits), _exponentBits(exponentBits) {}

    int mantissaBits() const { return _mantissaBits; }

    int exponentBits() const { return _exponentBits; }

    int maxExponent() const { return (1 << (_exponentBits - 1)) - 1; }

    int minExponent() const { return zeroExponent() + 1; }

    int zeroExponent() const { return -maxExponent(); }

private:
    int _mantissaBits;
    int _exponentBits;
};

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, APFloatPrecision precision);

} // namespace scatha

#endif // SCATHA_COMMON_APFWD_H_
