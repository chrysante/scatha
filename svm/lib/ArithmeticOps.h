#ifndef SVM_ARITHMETICOPS_H_
#define SVM_ARITHMETICOPS_H_

#include "Errors.h"

#define SVM_ARITHMETIC_OP(Name, ...)                                           \
    struct Name##_T {                                                          \
        auto operator()(__VA_ARGS__) const;                                    \
    };                                                                         \
    inline constexpr Name##_T Name{};                                          \
    inline auto Name##_T::operator()(__VA_ARGS__) const

namespace svm {

SVM_ARITHMETIC_OP(LogNot, auto x) { return !x; }

SVM_ARITHMETIC_OP(BitNot, auto x) { return ~x; }

SVM_ARITHMETIC_OP(Negate, auto x) { return -x; }

SVM_ARITHMETIC_OP(Add, auto x, auto y) { return x + y; }

SVM_ARITHMETIC_OP(Sub, auto x, auto y) { return x - y; }

SVM_ARITHMETIC_OP(Mul, auto x, auto y) { return x * y; }

SVM_ARITHMETIC_OP(Div, auto x, auto y) {
    if (y == decltype(y){ 0 }) {
        throwError<ArithmeticError>(ArithmeticError::DivideByZero);
    }
    return x / y;
}

SVM_ARITHMETIC_OP(Rem, auto x, auto y) {
    if (y == decltype(y){ 0 }) {
        throwError<ArithmeticError>(ArithmeticError::DivideByZero);
    }
    return x % y;
}

SVM_ARITHMETIC_OP(LSH, auto x, auto y) { return x << y; }

SVM_ARITHMETIC_OP(RSH, auto x, auto y) { return x >> y; }

SVM_ARITHMETIC_OP(ALSH, auto x, auto y) { return x << y; }

SVM_ARITHMETIC_OP(ARSH, auto x, auto y) {
    using S = std::make_signed_t<decltype(x)>;
    if (S(x) < 0 && y > 0)
        return S(x) >> y | ~(~S(0) >> y);
    else
        return S(x) >> y;
}

SVM_ARITHMETIC_OP(BitAnd, auto x, auto y) { return x & y; }

SVM_ARITHMETIC_OP(BitOr, auto x, auto y) { return x | y; }

SVM_ARITHMETIC_OP(BitXOr, auto x, auto y) { return x ^ y; }

} // namespace svm

#undef SVM_ARITHMETIC_OP

#endif // SVM_ARITHMETICOPS_H_
