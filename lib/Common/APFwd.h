#ifndef SCATHA_COMMON_APFWD_H_
#define SCATHA_COMMON_APFWD_H_

namespace scatha {

class APInt;
class APFloat;
	
enum class APFloatPrecision {
    Single     = 24,  //  32-bit floating point types have  23 mantissa bits
    Double     = 53,  //  64-bit floating point types have  64 mantissa bits
    LongDouble = 113, // 128-bit floating point types have 112 mantissa bits
    Default = Double
};

} // namespace scatha

#endif // SCATHA_COMMON_APFWD_H_

