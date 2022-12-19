#ifndef SCATHA_COMMON_APFLOAT_H_
#define SCATHA_COMMON_APFLOAT_H_

#include <iosfwd>
#include <string_view>
#include <span>
#include <concepts>
#include <compare>
#include <type_traits>
#include <optional>

#include "Basic/Basic.h"
#include "Common/APFwd.h"

namespace scatha {

class APFloat;

static auto* asImpl(APFloat const&);

SCATHA(API) APFloat operator+(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) APFloat operator-(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) APFloat operator*(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) APFloat operator/(APFloat const& lhs, APFloat const& rhs);

SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) inline bool operator==(APFloat const& lhs, APFloat const& rhs) {
    return (lhs <=> rhs) == std::strong_ordering::equal;
}

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, APFloat const& number);

class SCATHA(API) APFloat {
public:
    
    // MARK: Precision
    
    using Precision = APFloatPrecision;
    
    // MARK: Construction & Lifetime
    
    APFloat(Precision precision = Precision::Default);
    APFloat(std::signed_integral auto value, Precision precision = Precision::Default):
        APFloat(static_cast<long long>(value), precision) {}
    APFloat(long long value, Precision precision = Precision::Default);
    APFloat(std::unsigned_integral auto value, Precision precision = Precision::Default):
        APFloat(static_cast<unsigned long long>(value), precision) {}
    APFloat(unsigned long long value, Precision precision = Precision::Default);
    APFloat(float value, Precision precision = Precision::Default): APFloat(static_cast<double>(value), precision) {}
    APFloat(double value, Precision precision = Precision::Default);
    APFloat(long double value, Precision precision = Precision::Default);

    APFloat(APFloat const& rhs);
    APFloat(APFloat&& rhs) noexcept;
    APFloat& operator=(APFloat const& rhs);
    APFloat& operator=(APFloat&& rhs) noexcept;
    ~APFloat();

    template <typename T> requires std::is_arithmetic_v<T>
    explicit operator T() const;
    
    /// Convert a string to APFloat.
    ///
    /// \details Whitespaces are ignored.
    ///
    /// \param value String to convert.
    /// If \p value contains a dot \p '.' it must not have  a base-specifier prefix and will be parsed as a floating point value.
    ///
    /// \param base Must be either 0 or between 2 and 16. If \p base is 0, the leading characters are used for disambiguation:
    /// \p 0x or \p 0X for hex, \p 0b or \p 0B for binary, \p 0 for octal, or decimal otherwise.
    ///
    /// \returns The converted value if the conversion was successful. Empty optional otherwise.
    ///
    static std::optional<APFloat> parse(std::string_view value, int base = 0, Precision precision = Precision::Default);
    
    // MARK: Arithmetic
    
    APFloat& operator+=(APFloat const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    APFloat& operator+=(T rhs)& {
        return *this += APFloat(rhs);
    }
    
    APFloat& operator-=(APFloat const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    APFloat& operator-=(T rhs)& {
        return *this -= APFloat(rhs);
    }
    
    APFloat& operator*=(APFloat const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    APFloat& operator*=(T rhs)& {
        return *this *= APFloat(rhs);
    }
    
    APFloat& operator/=(APFloat const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    APFloat& operator/=(T rhs)& {
        return *this /= APFloat(rhs);
    }
    
    friend APFloat operator+(APFloat const& lhs, APFloat const& rhs);
    friend APFloat operator-(APFloat const& lhs, APFloat const& rhs);
    friend APFloat operator*(APFloat const& lhs, APFloat const& rhs);
    friend APFloat operator/(APFloat const& lhs, APFloat const& rhs);
    
    // MARK: Queries
    
    bool isInf() const;
    
    bool isNaN() const;
    
    Precision precision() const;
    
    ssize_t exponent() const;

    std::span<unsigned long> mantissa() const;

    std::string toString() const;

    void* getImplementationPointer() { return &storage; }
    
    void const* getImplementationPointer() const { return &storage; }
        
private:
    long long toSigned() const;
    unsigned long long toUnsigned() const;
    double toDouble() const;

    friend std::strong_ordering operator<=>(APFloat const& lhs, APFloat const& rhs);
    friend std::ostream& operator<<(std::ostream& ostream, APFloat const& number);
    friend auto* asImpl(APFloat const&);
    
private:
    std::aligned_storage_t<4 * sizeof(void*), alignof(void*)> storage;
    Precision prec;
};

} // namespace scatha

// MARK: Inline definitions

template <typename T> requires std::is_arithmetic_v<T>
inline scatha::APFloat::operator T() const {
    if constexpr (std::signed_integral<T>) {
        return static_cast<T>(toSigned());
    }
    else if constexpr (std::unsigned_integral<T>) {
        return static_cast<T>(toUnsigned());
    }
    else {
        static_assert(std::floating_point<T>);
        return static_cast<T>(toDouble());
    }
}

#endif // SCATHA_COMMON_APFLOAT_H_
