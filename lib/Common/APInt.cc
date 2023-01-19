#include "APInt.h"

#include <ostream>
#include <sstream>
#include <string>

#include <gmp.h>
#include <utl/scope_guard.hpp>
#include <utl/utility.hpp>

template <typename T>
static auto* as_mpz(T& value) {
    static_assert(sizeof(T) >= sizeof(mpz_t));
    if constexpr (std::is_const_v<T>) {
        return reinterpret_cast<mpz_srcptr>(&value);
    }
    else {
        return reinterpret_cast<mpz_ptr>(&value);
    }
}

using namespace scatha;

APInt::APInt() {
    mpz_init(as_mpz(storage));
}

APInt::APInt(long long value) {
    mpz_init_set_si(as_mpz(storage), value);
}

APInt::APInt(unsigned long long value) {
    mpz_init_set_ui(as_mpz(storage), value);
}

APInt::APInt(double value) {
    mpz_init_set_d(as_mpz(storage), value);
}

APInt::APInt(APInt const& rhs): APInt() {
    mpz_init_set(as_mpz(storage), as_mpz(rhs.storage));
}

APInt::APInt(APInt&& rhs) noexcept: APInt() {
    mpz_swap(as_mpz(storage), as_mpz(rhs.storage));
}

APInt& APInt::operator=(APInt const& rhs) {
    mpz_set(as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt& APInt::operator=(APInt&& rhs) noexcept {
    mpz_swap(as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt::~APInt() {
    mpz_clear(as_mpz(storage));
}

template <typename T>
requires std::is_arithmetic_v<T>
T scatha::APInt::to() const {
    if constexpr (std::signed_integral<T>) {
        return static_cast<T>(toSigned());
    }
    else if constexpr (std::unsigned_integral<T>) {
        if (*this >= 0) {
            return static_cast<T>(toUnsigned());
        }
        else {
            return static_cast<T>(toSigned());
        }
    }
    else {
        static_assert(std::floating_point<T>);
        return static_cast<T>(toDouble());
    }
}

template char APInt::to() const;
template signed char APInt::to() const;
template unsigned char APInt::to() const;
template signed short APInt::to() const;
template unsigned short APInt::to() const;
template signed int APInt::to() const;
template unsigned int APInt::to() const;
template signed long APInt::to() const;
template unsigned long APInt::to() const;
template signed long long APInt::to() const;
template unsigned long long APInt::to() const;
template float APInt::to() const;
template double APInt::to() const;
template long double APInt::to() const;

std::optional<APInt> APInt::fromString(std::string_view value, int base) {
    APInt result;
    int status = mpz_set_str(as_mpz(result.storage), value.data(), base);
    if (!status) {
        return std::move(result);
    }
    return std::nullopt;
}

APInt& APInt::operator+=(APInt const& rhs) & {
    mpz_add(as_mpz(storage), as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt& APInt::operator-=(APInt const& rhs) & {
    mpz_sub(as_mpz(storage), as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt& APInt::operator*=(APInt const& rhs) & {
    mpz_mul(as_mpz(storage), as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt& APInt::operator/=(APInt const& rhs) & {
    mpz_tdiv_q(as_mpz(storage), as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt& APInt::operator%=(APInt const& rhs) & {
    mpz_tdiv_r(as_mpz(storage), as_mpz(storage), as_mpz(rhs.storage));
    return *this;
}

APInt scatha::operator+(APInt const& lhs, APInt const& rhs) {
    APInt result = lhs;
    result += rhs;
    return result;
}

APInt scatha::operator-(APInt const& lhs, APInt const& rhs) {
    APInt result = lhs;
    result -= rhs;
    return result;
}

APInt scatha::operator*(APInt const& lhs, APInt const& rhs) {
    APInt result = lhs;
    result *= rhs;
    return result;
}

APInt scatha::operator/(APInt const& lhs, APInt const& rhs) {
    APInt result = lhs;
    result /= rhs;
    return result;
}

APInt scatha::operator%(APInt const& lhs, APInt const& rhs) {
    APInt result = lhs;
    result %= rhs;
    return result;
}

APInt scatha::operator+(APInt const& operand) {
    return operand;
}

APInt scatha::operator-(APInt const& operand) {
    return APInt(0) - operand;
}

APInt scatha::operator~(APInt const& operand) {
    APInt result(std::numeric_limits<uint64_t>::max());
    mpz_xor(as_mpz(result.storage), as_mpz(result.storage), as_mpz(operand.storage));
    return result;
}

APInt scatha::operator!(APInt const& operand) {
    return operand == 0 ? 1 : 0;
}

template <typename T>
bool APInt::representableAsImpl() const {
    if constexpr (std::is_same_v<T, unsigned long>) {
        return mpz_fits_ulong_p(as_mpz(storage));
    }
    else if constexpr (std::is_same_v<T, long>) {
        return mpz_fits_slong_p(as_mpz(storage));
    }
    else if constexpr (std::is_same_v<T, unsigned int>) {
        return mpz_fits_uint_p(as_mpz(storage));
    }
    else if constexpr (std::is_same_v<T, int>) {
        return mpz_fits_sint_p(as_mpz(storage));
    }
    else if constexpr (std::is_same_v<T, unsigned short>) {
        return mpz_fits_ushort_p(as_mpz(storage));
    }
    else if constexpr (std::is_same_v<T, short>) {
        return mpz_fits_sshort_p(as_mpz(storage));
    }
    else {
        return *this == static_cast<T>(*this);
    }
}

template bool APInt::representableAsImpl<char>() const;
template bool APInt::representableAsImpl<signed char>() const;
template bool APInt::representableAsImpl<unsigned char>() const;
template bool APInt::representableAsImpl<signed short>() const;
template bool APInt::representableAsImpl<unsigned short>() const;
template bool APInt::representableAsImpl<signed int>() const;
template bool APInt::representableAsImpl<unsigned int>() const;
template bool APInt::representableAsImpl<signed long>() const;
template bool APInt::representableAsImpl<unsigned long>() const;
template bool APInt::representableAsImpl<signed long long>() const;
template bool APInt::representableAsImpl<unsigned long long>() const;
template bool APInt::representableAsImpl<float>() const;
template bool APInt::representableAsImpl<double>() const;
template bool APInt::representableAsImpl<long double>() const;

long long APInt::toSigned() const {
    return mpz_get_si(as_mpz(storage));
}

unsigned long long APInt::toUnsigned() const {
    return mpz_get_ui(as_mpz(storage));
}

double APInt::toDouble() const {
    return mpz_get_d(as_mpz(storage));
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

std::strong_ordering scatha::operator<=>(APInt const& lhs, APInt const& rhs) {
    int const result = mpz_cmp(as_mpz(lhs.storage), as_mpz(rhs.storage));
    return toStrongOrdering(result);
}

std::strong_ordering scatha::operator<=>(APInt const& lhs, long long rhs) {
    int const result = mpz_cmp_si(as_mpz(lhs.storage), rhs);
    return toStrongOrdering(result);
}

std::strong_ordering scatha::operator<=>(APInt const& lhs, unsigned long long rhs) {
    int const result = mpz_cmp_ui(as_mpz(lhs.storage), rhs);
    return toStrongOrdering(result);
}

std::strong_ordering scatha::operator<=>(APInt const& lhs, double rhs) {
    int const result = mpz_cmp_d(as_mpz(lhs.storage), rhs);
    return toStrongOrdering(result);
}

std::string APInt::toString() const {
    /// Inefficient temporary solution.
    std::stringstream sstr;
    sstr << *this;
    return std::move(sstr).str();
}

std::ostream& scatha::operator<<(std::ostream& ostream, APInt const& number) {
    int const base = [&] {
        auto const flags = ostream.flags();
        return flags & std::ios::dec ? 10 :
               flags & std::ios::hex ? 16 :
                                       (SC_ASSERT(flags & std::ios::oct, "Must be 'oct' here"), 8);
    }();
    auto const size =
        mpz_sizeinbase(as_mpz(number.storage), base) + 2; // 2 extra chars for possible minus sign and null terminator.
    char* const buffer = static_cast<char*>(alloca(size));
    mpz_get_str(buffer, base, as_mpz(number.storage));
    return ostream << buffer;
}

std::size_t std::hash<scatha::APInt>::operator()(scatha::APInt const& value) const {
    auto const* const data = mpz_limbs_read(as_mpz(value.storage));
    size_t const size      = mpz_size(as_mpz(value.storage));
    static_assert(sizeof(*data) == 8);
    uint64_t result = 0;
    for (size_t i = 0; i < size; ++i) {
        result ^= data[i];
    }
    return result;
}
