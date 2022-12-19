#include "APFloat.h"

#include <ostream>
#include <sstream>
#include <string>

#include <mpfr.h>
#include <utl/utility.hpp>
#include <utl/scope_guard.hpp>

template <typename T>
static auto* as_mpfr(T& value) {
    static_assert(sizeof(T) >= sizeof(mpfr_t));
    if constexpr (std::is_const_v<T>) {
        return reinterpret_cast<mpfr_srcptr>(&value);
    }
    else {
        return reinterpret_cast<mpfr_ptr>(&value);
    }
}

namespace scatha {

APFloat::APFloat(Precision precision) {
    mpfr_init2(as_mpfr(storage), static_cast<mpfr_prec_t>(precision));
}

APFloat::APFloat(long long value, Precision precision): APFloat(precision) {
    mpfr_set_si(as_mpfr(storage), value, MPFR_RNDN);
}

APFloat::APFloat(unsigned long long value, Precision precision): APFloat(precision) {
    mpfr_init_set_ui(as_mpfr(storage), value, MPFR_RNDN);
}

APFloat::APFloat(double value, Precision precision): APFloat(precision) {
    mpfr_set_d(as_mpfr(storage), value, MPFR_RNDN);
}

APFloat::APFloat(long double value, Precision precision): APFloat(precision) {
    mpfr_set_ld(as_mpfr(storage), value, MPFR_RNDN);
}

APFloat::APFloat(APFloat const& rhs) {
    mpfr_init_set(as_mpfr(storage), as_mpfr(rhs.storage), MPFR_RNDN);
}

APFloat::APFloat(APFloat&& rhs) noexcept: APFloat() {
    mpfr_swap(as_mpfr(storage), as_mpfr(rhs.storage));
}

APFloat& APFloat::operator=(APFloat const& rhs) {
    mpfr_set(as_mpfr(storage), as_mpfr(rhs.storage), MPFR_RNDN);
    return *this;
}

APFloat& APFloat::operator=(APFloat&& rhs) noexcept {
    mpfr_swap(as_mpfr(storage), as_mpfr(rhs.storage));
    return *this;
}

APFloat::~APFloat() {
    mpfr_clear(as_mpfr(storage));
}

std::optional<APFloat> APFloat::parse(std::string_view value, int base, Precision precision) {
    APFloat result(precision);
    int status = mpfr_set_str(as_mpfr(result.storage), value.data(), base, MPFR_RNDN);
    if (status == 0) {
        return std::move(result);
    }
    return std::nullopt;
}


APFloat& APFloat::operator+=(APFloat const& rhs)& {
    mpfr_add(as_mpfr(storage), as_mpfr(storage), as_mpfr(rhs.storage), MPFR_RNDN);
    return *this;
}

APFloat& APFloat::operator-=(APFloat const& rhs)& {
    mpfr_sub(as_mpfr(storage), as_mpfr(storage), as_mpfr(rhs.storage), MPFR_RNDN);
    return *this;
}

APFloat& APFloat::operator*=(APFloat const& rhs)& {
    mpfr_mul(as_mpfr(storage), as_mpfr(storage), as_mpfr(rhs.storage), MPFR_RNDN);
    return *this;
}

APFloat& APFloat::operator/=(APFloat const& rhs)& {
    mpfr_div(as_mpfr(storage), as_mpfr(storage), as_mpfr(rhs.storage), MPFR_RNDN);
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

long long APFloat::toSigned() const {
    return mpfr_get_si(as_mpfr(storage), MPFR_RNDN);
}

unsigned long long APFloat::toUnsigned() const {
    return mpfr_get_ui(as_mpfr(storage), MPFR_RNDN);
}

double APFloat::toDouble() const {
    return mpfr_get_d(as_mpfr(storage), MPFR_RNDN);
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
    int const result = mpfr_cmp(as_mpfr(lhs.storage), as_mpfr(rhs.storage));
    return toStrongOrdering(result);
}

APFloat::Precision APFloat::precision() const {
    return static_cast<Precision>(mpfr_get_prec(as_mpfr(storage)));
}

ssize_t APFloat::exponent() const {
    return as_mpfr(storage)->_mpfr_exp;
};

std::span<unsigned long> APFloat::mantissa() const {
    size_t const prec = utl::narrow_cast<size_t>((as_mpfr(storage)->_mpfr_prec));
    size_t const size = prec / GMP_NUMB_BITS + !!(prec % GMP_NUMB_BITS);
    return std::span(as_mpfr(storage)->_mpfr_d, size);
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
    char* const tmpString = mpfr_get_str(nullptr, &exponent, base, 0, as_mpfr(number.storage), MPFR_RNDN);
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
    /// Pop trailing zeros.
    while (stringValue.size() > 2 &&
           stringValue[stringValue.size() - 1] == '0' &&
           stringValue[stringValue.size() - 2] != '.')
    {
        stringValue.pop_back();
    }
    return ostream << stringValue;
}

} // namespace scatha
