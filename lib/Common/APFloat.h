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

namespace scatha {

class APFloat;

SCATHA(API) APFloat operator+(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) APFloat operator-(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) APFloat operator*(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) APFloat operator/(APFloat const& lhs, APFloat const& rhs);

SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, APFloat const& rhs);
SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, long long rhs);
SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, unsigned long long rhs);
SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, double rhs);
SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, std::signed_integral auto rhs);
SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, std::unsigned_integral auto rhs);
SCATHA(API) std::strong_ordering operator<=>(APFloat const& lhs, std::floating_point auto rhs);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, APFloat const& number);

class SCATHA(API) APFloat {
public:
    // MARK: Construction & Lifetime
    APFloat(size_t precisionBits = 64);
    APFloat(std::signed_integral auto value):
        APFloat(static_cast<long long>(value)) {}
    APFloat(long long value);
    APFloat(std::unsigned_integral auto value):
        APFloat(static_cast<unsigned long long>(value)) {}
    APFloat(unsigned long long value);
    APFloat(double value, size_t precisionBits = 64);

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
    static std::optional<APFloat> parse(std::string_view value, int base = 0);
    
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
    
    /// Query this number for lossless convertability to C++ arithmetic types.
    ///
    /// \returns \p true iff \p *this is losslessly convertible to \p T
    template <typename T> requires std::is_arithmetic_v<T>
    bool representableAs() const { return representableAsImpl<T>(); }
    
    bool isIntegral() const;
    
    std::string toString() const;
    
    void* getImplementationPointer() { return &storage; }
    void const* getImplementationPointer() const { return &storage; }
    
    ssize_t exponent() const;
    std::span<unsigned long> mantissa() const;
    
    friend std::strong_ordering operator<=>(APFloat const& lhs, APFloat const& rhs);
    friend std::strong_ordering operator<=>(APFloat const& lhs, long long rhs);
    friend std::strong_ordering operator<=>(APFloat const& lhs, unsigned long long rhs);
    friend std::strong_ordering operator<=>(APFloat const& lhs, double rhs);
    
    friend bool operator==(APFloat const& lhs, APFloat const& rhs) {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }
    
    template <typename T> requires std::is_arithmetic_v<T>
    friend bool operator==(APFloat const& lhs, T rhs) {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }
    
    friend std::ostream& operator<<(std::ostream& ostream, APFloat const& number);
    
private:
    template <typename T>
    bool representableAsImpl() const;
    
    long long toSigned() const;
    unsigned long long toUnsigned() const;
    double toDouble() const;
    
private:
    std::aligned_storage_t<3 * sizeof(void*), alignof(void*)> storage;
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

std::strong_ordering scatha::operator<=>(APFloat const& lhs, std::signed_integral auto rhs) {
    return operator<=>(lhs, static_cast<long long>(rhs));
}

std::strong_ordering scatha::operator<=>(APFloat const& lhs, std::unsigned_integral auto rhs) {
    return operator<=>(lhs, static_cast<unsigned long long>(rhs));
}

std::strong_ordering scatha::operator<=>(APFloat const& lhs, std::floating_point auto rhs) {
    return operator<=>(lhs, static_cast<double>(rhs));
}

#endif // SCATHA_COMMON_APFLOAT_H_
