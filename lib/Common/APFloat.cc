#include "APFloat.h"

#include <ostream>
#include <sstream>
#include <string>

#include <mpfr.h>
#include <utl/utility.hpp>
#include <utl/scope_guard.hpp>

using namespace scatha;

static auto* scatha::asImpl(APFloat const& f) {
    static_assert(sizeof(f.storage) >= sizeof(mpfr_t));
    return reinterpret_cast<mpfr_srcptr>(&f.storage);
}

namespace scatha {

static auto* asImpl(APFloat& f) { return const_cast<mpfr_ptr>(asImpl(static_cast<APFloat const&>(f))); }

} // namespace scatha

static constexpr auto roundingMode = MPFR_RNDN;

/// This unfortunately is global.
static void setExponentRange(APFloatPrecision prec) {
    /// Weirdly enough mpfr expects these values to be 1 less/greater than the actual values.
    mpfr_set_emin(prec.minExponent() - 1);
    mpfr_set_emax(prec.maxExponent() + 1);
}

APFloat::APFloat(Precision precision): prec(precision) {
    setExponentRange(prec);
    mpfr_init2(asImpl(*this), prec.mantissaBits() + 1);
}

APFloat::APFloat(long long value, Precision precision): APFloat(precision) {
    mpfr_set_si(asImpl(*this), value, roundingMode);
}

APFloat::APFloat(unsigned long long value, Precision precision): APFloat(precision) {
    mpfr_init_set_ui(asImpl(*this), value, roundingMode);
}

APFloat::APFloat(double value, Precision precision): APFloat(precision) {
    mpfr_set_d(asImpl(*this), value, roundingMode);
}

APFloat::APFloat(long double value, Precision precision): APFloat(precision) {
    mpfr_set_ld(asImpl(*this), value, roundingMode);
}

APFloat::APFloat(APFloat const& rhs): prec(rhs.precision()) {
    setExponentRange(prec);
    mpfr_init_set(asImpl(*this), asImpl(rhs), roundingMode);
}

APFloat::APFloat(APFloat&& rhs) noexcept: APFloat(rhs.precision()) {
    mpfr_swap(asImpl(*this), asImpl(rhs));
}

APFloat& APFloat::operator=(APFloat const& rhs) {
    mpfr_set(asImpl(*this), asImpl(rhs), roundingMode);
    return *this;
}

APFloat& APFloat::operator=(APFloat&& rhs) noexcept {
    mpfr_swap(asImpl(*this), asImpl(rhs));
    return *this;
}

APFloat::~APFloat() {
    mpfr_clear(asImpl(*this));
}

std::optional<APFloat> APFloat::parse(std::string_view value, int base, Precision precision) {
    APFloat result(precision);
    int status = mpfr_set_str(asImpl(result), value.data(), base, roundingMode);
    if (status == 0) {
        SC_ASSERT( result.isInf()                                            ||
                   result.isNaN()                                            ||
                   result.exponent() == std::numeric_limits<long>::min() + 1 || // When result == 0
                  (result.exponent() <= precision.maxExponent() &&
                   result.exponent() >= precision.minExponent()), "");
        return std::move(result);
    }
    return std::nullopt;
}


APFloat& APFloat::operator+=(APFloat const& rhs)& {
    mpfr_add(asImpl(*this), asImpl(*this), asImpl(rhs), roundingMode);
    return *this;
}

APFloat& APFloat::operator-=(APFloat const& rhs)& {
    mpfr_sub(asImpl(*this), asImpl(*this), asImpl(rhs), roundingMode);
    return *this;
}

APFloat& APFloat::operator*=(APFloat const& rhs)& {
    mpfr_mul(asImpl(*this), asImpl(*this), asImpl(rhs), roundingMode);
    return *this;
}

APFloat& APFloat::operator/=(APFloat const& rhs)& {
    mpfr_div(asImpl(*this), asImpl(*this), asImpl(rhs), roundingMode);
    return *this;
}

APFloat scatha::operator+(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result += rhs;
    return result;
}

APFloat scatha::operator-(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result -= rhs;
    return result;
}

APFloat scatha::operator*(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result *= rhs;
    return result;
}

APFloat scatha::operator/(APFloat const& lhs, APFloat const& rhs) {
    APFloat result = lhs;
    result /= rhs;
    return result;
}

long long APFloat::toSigned() const {
    return mpfr_get_si(asImpl(*this), roundingMode);
}

unsigned long long APFloat::toUnsigned() const {
    return mpfr_get_ui(asImpl(*this), roundingMode);
}

double APFloat::toDouble() const {
    return mpfr_get_d(asImpl(*this), roundingMode);
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

std::strong_ordering scatha::operator<=>(APFloat const& lhs, APFloat const& rhs) {
    int const result = mpfr_cmp(asImpl(lhs), asImpl(rhs));
    return toStrongOrdering(result);
}

bool APFloat::isInf() const {
    return mpfr_inf_p(asImpl(*this)) != 0;
}

bool APFloat::isNaN() const {
    return mpfr_nan_p(asImpl(*this)) != 0;
}

APFloat::Precision APFloat::precision() const {
    SC_ASSERT(mpfr_get_prec(asImpl(*this)) == prec.mantissaBits() + 1, "");
    return prec;
}

ssize_t APFloat::exponent() const {
    return asImpl(*this)->_mpfr_exp;
};

std::span<unsigned long> APFloat::mantissa() const {
    size_t const prec = utl::narrow_cast<size_t>((asImpl(*this)->_mpfr_prec));
    size_t const size = prec / GMP_NUMB_BITS + !!(prec % GMP_NUMB_BITS);
    return std::span(asImpl(*this)->_mpfr_d, size);
}

std::string APFloat::toString() const {
    /// Inefficient temporary solution.
    std::stringstream sstr;
    sstr << *this;
    return std::move(sstr).str();
}

std::ostream& scatha::operator<<(std::ostream& ostream, APFloat const& number) {
    if (number.isInf()) {
        return ostream << (number > 0 ? "" : "-") << "inf";
    }
    if (number.isNaN()) {
        return ostream << "nan";
    }
    int const base = [&] {
        auto const flags = ostream.flags();
        return flags & std::ios::dec ? 10 :
               flags & std::ios::hex ? 16 :
                                       (SC_ASSERT(flags & std::ios::oct,
                                                  "Must be 'oct' here"),
                                        8);
    }();
    mp_exp_t exponent{};
    char* const tmpString = mpfr_get_str(nullptr, &exponent, base, 0, asImpl(number), roundingMode);
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
    if (std::find(stringValue.begin(), stringValue.end(), '.') != stringValue.end()) {
        /// Pop trailing zeros if we have a dot.
        while (stringValue.size() > 2 &&
               stringValue[stringValue.size() - 1] == '0' &&
               stringValue[stringValue.size() - 2] != '.')
        {
            stringValue.pop_back();
        }
    }
    return ostream << stringValue;
}
