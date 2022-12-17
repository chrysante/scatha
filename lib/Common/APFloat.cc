#include "APFloat.h"

#include <ostream>
#include <sstream>
#include <string>

#include <gmp.h>
#include <utl/utility.hpp>
#include <utl/scope_guard.hpp>

template <typename T>
static auto* as_mpf(T& value) {
    static_assert(sizeof(T) >= sizeof(mpf_t));
    if constexpr (std::is_const_v<T>) {
        return reinterpret_cast<mpf_srcptr>(&value);
    }
    else {
        return reinterpret_cast<mpf_ptr>(&value);
    }
}

namespace scatha {

APFloat::APFloat(size_t precisionBits) {
    mpf_init2(as_mpf(storage), precisionBits);
}

APFloat::APFloat(long long value) {
    mpf_init_set_si(as_mpf(storage), value);
}

APFloat::APFloat(unsigned long long value) {
    mpf_init_set_ui(as_mpf(storage), value);
}

APFloat::APFloat(double value, size_t precisionBits): APFloat(precisionBits) {
    mpf_set_d(as_mpf(storage), value);
}

APFloat::APFloat(APFloat const& rhs) {
    mpf_init_set(as_mpf(storage), as_mpf(rhs.storage));
}

APFloat::APFloat(APFloat&& rhs) noexcept: APFloat() {
    mpf_swap(as_mpf(storage), as_mpf(rhs.storage));
}

APFloat& APFloat::operator=(APFloat const& rhs) {
    mpf_set(as_mpf(storage), as_mpf(rhs.storage));
    return *this;
}

APFloat& APFloat::operator=(APFloat&& rhs) noexcept {
    mpf_swap(as_mpf(storage), as_mpf(rhs.storage));
    return *this;
}

APFloat::~APFloat() {
    mpf_clear(as_mpf(storage));
}

std::optional<APFloat> APFloat::parse(std::string_view value, int base) {
    APFloat result;
    int status = mpf_set_str(as_mpf(result.storage), value.data(), base);
    if (!status) {
        return std::move(result);
    }
    return std::nullopt;
}


APFloat& APFloat::operator+=(APFloat const& rhs)& {
    mpf_add(as_mpf(storage), as_mpf(storage), as_mpf(rhs.storage));
    return *this;
}

APFloat& APFloat::operator-=(APFloat const& rhs)& {
    mpf_sub(as_mpf(storage), as_mpf(storage), as_mpf(rhs.storage));
    return *this;
}

APFloat& APFloat::operator*=(APFloat const& rhs)& {
    mpf_mul(as_mpf(storage), as_mpf(storage), as_mpf(rhs.storage));
    return *this;
}

APFloat& APFloat::operator/=(APFloat const& rhs)& {
    mpf_div(as_mpf(storage), as_mpf(storage), as_mpf(rhs.storage));
    return *this;
}

APFloat operator+(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result += rhs;
    return result;
}

APFloat operator-(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result -= rhs;
    return result;
}

APFloat operator*(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result *= rhs;
    return result;
}

APFloat operator/(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result /= rhs;
    return result;
}

template <typename T>
bool APFloat::representableAsImpl() const {
    if constexpr (std::is_same_v<T, unsigned long>) {
        return mpf_fits_ulong_p(as_mpf(storage)) && isIntegral();
    }
    else if constexpr (std::is_same_v<T, long>) {
        return mpf_fits_slong_p(as_mpf(storage)) && isIntegral();
    }
    else if constexpr (std::is_same_v<T, unsigned int>) {
        return mpf_fits_uint_p(as_mpf(storage)) && isIntegral();
    }
    else if constexpr (std::is_same_v<T, int>) {
        return mpf_fits_sint_p(as_mpf(storage)) && isIntegral();
    }
    else if constexpr (std::is_same_v<T, unsigned short>) {
        return mpf_fits_ushort_p(as_mpf(storage)) && isIntegral();
    }
    else if constexpr (std::is_same_v<T, short>) {
        return mpf_fits_sshort_p(as_mpf(storage)) && isIntegral();
    }
    else {
        return *this == static_cast<T>(*this);
    }
}

template bool APFloat::representableAs<         char>() const;
template bool APFloat::representableAs<  signed char>() const;
template bool APFloat::representableAs<unsigned char>() const;
template bool APFloat::representableAs<  signed short>() const;
template bool APFloat::representableAs<unsigned short>() const;
template bool APFloat::representableAs<  signed int>() const;
template bool APFloat::representableAs<unsigned int>() const;
template bool APFloat::representableAs<  signed long>() const;
template bool APFloat::representableAs<unsigned long>() const;
template bool APFloat::representableAs<  signed long long>() const;
template bool APFloat::representableAs<unsigned long long>() const;
template bool APFloat::representableAs<float>() const;
template bool APFloat::representableAs<double>() const;
template bool APFloat::representableAs<long double>() const;

bool APFloat::isIntegral() const {
    mpf_t floor;
    mpf_init(floor);
    mpf_floor(floor, as_mpf(storage));
    return mpf_cmp(as_mpf(storage), floor) == 0;
}

long long APFloat::toSigned() const {
    return mpf_get_si(as_mpf(storage));
}

unsigned long long APFloat::toUnsigned() const {
    return mpf_get_ui(as_mpf(storage));
}

double APFloat::toDouble() const {
    return mpf_get_d(as_mpf(storage));
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

std::strong_ordering operator<=>(APFloat const& lhs, APFloat const& rhs) {
    int const result = mpf_cmp(as_mpf(lhs.storage), as_mpf(rhs.storage));
    return toStrongOrdering(result);
}

std::strong_ordering operator<=>(APFloat const& lhs, long long rhs) {
    int const result = mpf_cmp_si(as_mpf(lhs.storage), rhs);
    return toStrongOrdering(result);
}

std::strong_ordering operator<=>(APFloat const& lhs, unsigned long long rhs) {
    int const result = mpf_cmp_ui(as_mpf(lhs.storage), rhs);
    return toStrongOrdering(result);
}

std::strong_ordering operator<=>(APFloat const& lhs, double rhs) {
    int const result = mpf_cmp_d(as_mpf(lhs.storage), rhs);
    return toStrongOrdering(result);
}

ssize_t APFloat::exponent() const {
    return as_mpf(storage)->_mp_exp;
};

std::span<unsigned long> APFloat::mantissa() const {
    return std::span(as_mpf(storage)->_mp_d, utl::narrow_cast<std::size_t>(as_mpf(storage)->_mp_size));
}

std::string APFloat::toString() const {
    /// Inefficient temporary solution.
    std::stringstream sstr;
    sstr << *this;
    return std::move(sstr).str();
}

std::ostream& operator<<(std::ostream& ostream, APFloat const& number) {
    int const base = [&] {
        auto const flags = ostream.flags();
        return flags & std::ios::dec ? 10 :
               flags & std::ios::hex ? 16 :
                                       (SC_ASSERT(flags & std::ios::oct,
                                                  "Must be 'oct' here"),
                                        8);
    }();
    mp_exp_t exponent{};
    char* const tmpString = mpf_get_str(nullptr, &exponent, base, 0, as_mpf(number.storage));
    utl::scope_guard free = [&]{ std::free(tmpString); };
    std::string stringValue(tmpString);
    bool const negative = stringValue.front() == '-';
    std::size_t const beginPos = negative ? 1 : 0;
    std::size_t stringSize = stringValue.size() - negative;
    
    if (exponent > 0) {
        while (stringSize < exponent) {
            stringValue.push_back('0');
            stringSize = stringValue.size() - negative;
        }
        if (stringSize > exponent) {
            stringValue.insert(beginPos + utl::narrow_cast<size_t>(exponent), ".");
        }
    }
    else {
        stringValue.insert(beginPos, utl::narrow_cast<size_t>(-exponent), '0');
        stringValue.insert(beginPos, "0.");
    }
    return ostream << stringValue;
}

} // namespace scatha
