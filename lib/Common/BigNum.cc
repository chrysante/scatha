#include "BigNum.h"

#include <utl/utility.hpp>
#include <utl/scope_guard.hpp>

#include <ostream>
#include <sstream>
#include <string>

#include <gmp.h>

template <typename T>
static auto* as_mpq(T& value) {
    static_assert(sizeof(T) == sizeof(mpq_t));
    if constexpr (std::is_const_v<T>) {
        return reinterpret_cast<mpq_srcptr>(&value);
    }
    else {
        return reinterpret_cast<mpq_ptr>(&value);
    }
}

namespace scatha {

BigNum::BigNum() {
    mpq_init(as_mpq(storage));
}

BigNum::BigNum(long long value): BigNum() {
    mpq_set_si(as_mpq(storage), value, 1);
}

BigNum::BigNum(unsigned long long value): BigNum() {
    mpq_set_ui(as_mpq(storage), value, 1);
}

BigNum::BigNum(double value): BigNum() {
    mpq_set_d(as_mpq(storage), value);
}

BigNum::BigNum(BigNum const& rhs): BigNum() {
    mpq_set(as_mpq(storage), as_mpq(rhs.storage));
}

BigNum::BigNum(BigNum&& rhs) noexcept: BigNum() {
    mpq_swap(as_mpq(storage), as_mpq(rhs.storage));
}

BigNum& BigNum::operator=(BigNum const& rhs) {
    mpq_set(as_mpq(storage), as_mpq(rhs.storage));
    return *this;
}

BigNum& BigNum::operator=(BigNum&& rhs) noexcept {
    mpq_swap(as_mpq(storage), as_mpq(rhs.storage));
    return *this;
}

BigNum::~BigNum() {
    mpq_clear(as_mpq(storage));
}

std::optional<BigNum> BigNum::fromString(std::string_view value, int base) {
    BigNum result;
    int status = mpq_set_str(as_mpq(result.storage), value.data(), base);
    if (!status) { return std::move(result); }
    /// If we fail, we try to parse a floating point number.
    mpf_t f;
    status = mpf_init_set_str(f, value.data(), base);
    utl::scope_guard clearF = [&]{ mpf_clear(f); };
    if (!status) {
        mpq_set_f(as_mpq(result.storage), f);
        return std::move(result);
    }
    return std::nullopt;
}

BigNum& BigNum::operator+=(BigNum const& rhs)& {
    mpq_add(as_mpq(storage), as_mpq(storage), as_mpq(rhs.storage));
    return *this;
}

BigNum& BigNum::operator-=(BigNum const& rhs)& {
    mpq_sub(as_mpq(storage), as_mpq(storage), as_mpq(rhs.storage));
    return *this;
}

BigNum& BigNum::operator*=(BigNum const& rhs)& {
    mpq_mul(as_mpq(storage), as_mpq(storage), as_mpq(rhs.storage));
    SC_ASSERT(isCanonical(), "");
    return *this;
}

BigNum& BigNum::operator/=(BigNum const& rhs)& {
    mpq_div(as_mpq(storage), as_mpq(storage), as_mpq(rhs.storage));
    SC_ASSERT(isCanonical(), "");
    return *this;
}

BigNum operator+(BigNum const& lhs, BigNum const& rhs) {
    BigNum result = lhs;
    result += rhs;
    return result;
}

BigNum operator-(BigNum const& lhs, BigNum const& rhs) {
    BigNum result = lhs;
    result -= rhs;
    return result;
}

BigNum operator*(BigNum const& lhs, BigNum const& rhs) {
    BigNum result = lhs;
    result *= rhs;
    return result;
}

BigNum operator/(BigNum const& lhs, BigNum const& rhs) {
    BigNum result = lhs;
    result /= rhs;
    return result;
}

template <typename T>
bool BigNum::representableAsImpl() const {
    if constexpr (std::integral<T>) {
        if (!isIntegral()) { return false; }
        mpz_srcptr num = mpq_numref(as_mpq(storage));
        auto constexpr cmpFn = [](mpz_srcptr lhs, T rhs) {
            if constexpr (std::signed_integral<T>) { return mpz_cmp_si(lhs, rhs); }
            else { return mpz_cmp_ui(lhs, rhs); }
        };
        return cmpFn(num, std::numeric_limits<T>::min()) >= 0 &&
               cmpFn(num, std::numeric_limits<T>::max()) <= 0;
    }
    else {
        static_assert(std::floating_point<T>, "T must be either integral or floating point type.");
        double const doubleValue = mpq_get_d(as_mpq(storage));
        bool const representableAsDouble = *this == BigNum(doubleValue);
        if constexpr (std::is_same_v<T, double> || std::is_same_v<T, long double>) {
            return representableAsDouble;
        }
        else {
            static_assert(std::is_same_v<T, float>);
            if (!representableAsDouble) { return false; }
            float const floatValue = static_cast<float>(doubleValue);
            bool const representableAsFloat = floatValue == doubleValue;
            return representableAsFloat;
        }
    }
}

template bool BigNum::representableAs<         char>() const;
template bool BigNum::representableAs<  signed char>() const;
template bool BigNum::representableAs<unsigned char>() const;
template bool BigNum::representableAs<  signed short>() const;
template bool BigNum::representableAs<unsigned short>() const;
template bool BigNum::representableAs<  signed int>() const;
template bool BigNum::representableAs<unsigned int>() const;
template bool BigNum::representableAs<  signed long>() const;
template bool BigNum::representableAs<unsigned long>() const;
template bool BigNum::representableAs<  signed long long>() const;
template bool BigNum::representableAs<unsigned long long>() const;
template bool BigNum::representableAs<float>() const;
template bool BigNum::representableAs<double>() const;
template bool BigNum::representableAs<long double>() const;

bool BigNum::isIntegral() const {
    SC_ASSERT(isCanonical(), "Number must be canonical for the test to work."
                             "However we cannot canonicalize here as this function is const.");
    mpz_srcptr denom = mpq_denref(as_mpq(storage));
    return mpz_cmp_si(denom, 1) == 0;
}

void BigNum::canonicalize() {
    mpq_canonicalize(as_mpq(storage));
}

bool BigNum::isCanonical() const {
    mpz_t gcd;
    mpz_init(gcd);
    mpz_gcd(gcd, mpq_numref(as_mpq(storage)), mpq_denref(as_mpq(storage)));
    return mpz_cmp_si(gcd, 1) == 0;
}

template <typename T>
static T convertToIntegral(mpq_srcptr q) {
    mpz_t z;
    mpz_init(z);
    utl::scope_guard clearZ = [&]{ mpz_clear(z); };
    mpq_get_num(z, q);
    if constexpr (std::signed_integral<T>) {
        return mpz_get_si(z);
    }
    else {
        return mpz_get_ui(z);
    }
}

long long BigNum::toSigned() const {
    if (isIntegral()) {
        return convertToIntegral<long long>(as_mpq(storage));
    }
    else {
        return static_cast<long long>(toDouble());
    }
}

unsigned long long BigNum::toUnsigned() const {
    if (isIntegral()) {
        return convertToIntegral<unsigned long long>(as_mpq(storage));
    }
    else {
        return static_cast<unsigned long long>(toDouble());
    }
}

double BigNum::toDouble() const {
    return mpq_get_d(as_mpq(storage));
}

static std::strong_ordering toStrongOrdering(int value) {
    if (value < 0) {
        return std::strong_ordering::less;
    }
    else if (value == 0) {
        return std::strong_ordering::equal;
    }
    else {
        return std::strong_ordering::greater;
    }
}

std::strong_ordering operator<=>(BigNum const& lhs, BigNum const& rhs) {
    int const result = mpq_cmp(as_mpq(lhs.storage), as_mpq(rhs.storage));
    return toStrongOrdering(result);
}

std::strong_ordering operator<=>(BigNum const& lhs, long long rhs) {
    int const result = mpq_cmp_si(as_mpq(lhs.storage), rhs, 1);
    return toStrongOrdering(result);
}

std::strong_ordering operator<=>(BigNum const& lhs, unsigned long long rhs) {
    int const result = mpq_cmp_ui(as_mpq(lhs.storage), rhs, 1);
    return toStrongOrdering(result);
}

std::strong_ordering operator<=>(BigNum const& lhs, double rhs) {
    return lhs <=> BigNum(rhs);
}

static void printAsIntegral(std::ostream& ostream, mpz_srcptr op, int base) {
    size_t const stringSize = mpz_sizeinbase(op, base) + 2;
    char* const string = static_cast<char*>(alloca(stringSize));
    mpz_get_str(string, base, op);
    ostream << string;
}

static void printAsFloat(std::ostream& ostream, mpq_srcptr op, int base) {
    mpf_t f;
    mpf_init(f);
    mpf_set_q(f, op);
    utl::scope_guard clear = [&]{ mpf_clear(f); };
    mp_exp_t exp;
    char* const tmpString = mpf_get_str(nullptr, &exp, base, 0, f);
    utl::scope_guard free = [&]{ std::free(tmpString); };
    std::string stringValue(tmpString);
    stringValue.insert(utl::narrow_cast<size_t>(exp + (stringValue.front() == '-')), ".");
    ostream << stringValue;
}

std::string BigNum::toString() const {
    /// Inefficient temporary solution.
    std::stringstream sstr;
    sstr << *this;
    return std::move(sstr).str();
}

std::ostream& operator<<(std::ostream& ostream, BigNum const& number) {
    int const base = [&] {
        auto const flags = ostream.flags();
        return flags & std::ios::dec ? 10 :
               flags & std::ios::hex ? 16 :
                                       (SC_ASSERT(flags & std::ios::oct,
                                                  "Must be 'oct' here"),
                                        8);
    }();
    if (number.isIntegral()) {
        printAsIntegral(ostream, mpq_numref(as_mpq(number.storage)), base);
    }
    else {
        printAsFloat(ostream, as_mpq(number.storage), base);
    }
    return ostream;
}

} // namespace scatha
