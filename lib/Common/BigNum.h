#ifndef SCATHA_COMMON_BIGNUM_H_
#define SCATHA_COMMON_BIGNUM_H_

#include <iosfwd>
#include <string_view>
#include <concepts>
#include <compare>
#include <type_traits>
#include <optional>

#include "Basic/Basic.h"

namespace scatha {

class BigNum;

SCATHA(API) BigNum operator+(BigNum const& lhs, BigNum const& rhs);
SCATHA(API) BigNum operator-(BigNum const& lhs, BigNum const& rhs);
SCATHA(API) BigNum operator*(BigNum const& lhs, BigNum const& rhs);
SCATHA(API) BigNum operator/(BigNum const& lhs, BigNum const& rhs);

SCATHA(API) std::strong_ordering operator<=>(BigNum const& lhs, BigNum const& rhs);
SCATHA(API) std::strong_ordering operator<=>(BigNum const& lhs, long long rhs);
SCATHA(API) std::strong_ordering operator<=>(BigNum const& lhs, unsigned long long rhs);
SCATHA(API) std::strong_ordering operator<=>(BigNum const& lhs, double rhs);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, BigNum const& number);

class SCATHA(API) BigNum {
public:
    // MARK: Construction & Lifetime
    BigNum();
    BigNum(std::signed_integral auto value):
        BigNum(static_cast<long long>(value)) {}
    BigNum(long long value);
    BigNum(std::unsigned_integral auto value):
        BigNum(static_cast<unsigned long long>(value)) {}
    BigNum(unsigned long long value);
    BigNum(double value);

    BigNum(BigNum const& rhs);
    BigNum(BigNum&& rhs) noexcept;
    BigNum& operator=(BigNum const& rhs);
    BigNum& operator=(BigNum&& rhs) noexcept;
    ~BigNum();

    template <typename T> requires std::is_arithmetic_v<T>
    explicit operator T() const;
    
    /// Convert a string to BigNum.
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
    static std::optional<BigNum> fromString(std::string_view value, int base = 0);
    
    // MARK: Arithmetic
    
    BigNum& operator+=(BigNum const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    BigNum& operator+=(T rhs)& {
        return *this += BigNum(rhs);
    }
    
    BigNum& operator-=(BigNum const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    BigNum& operator-=(T rhs)& {
        return *this -= BigNum(rhs);
    }
    
    BigNum& operator*=(BigNum const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    BigNum& operator*=(T rhs)& {
        return *this *= BigNum(rhs);
    }
    
    BigNum& operator/=(BigNum const& rhs)&;
    template <typename T> requires std::is_arithmetic_v<T>
    BigNum& operator/=(T rhs)& {
        return *this /= BigNum(rhs);
    }
    
    friend BigNum operator+(BigNum const& lhs, BigNum const& rhs);
    friend BigNum operator-(BigNum const& lhs, BigNum const& rhs);
    friend BigNum operator*(BigNum const& lhs, BigNum const& rhs);
    friend BigNum operator/(BigNum const& lhs, BigNum const& rhs);
    
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
    
    friend std::strong_ordering operator<=>(BigNum const& lhs, BigNum const& rhs);
    friend std::strong_ordering operator<=>(BigNum const& lhs, long long rhs);
    friend std::strong_ordering operator<=>(BigNum const& lhs, unsigned long long rhs);
    friend std::strong_ordering operator<=>(BigNum const& lhs, double rhs);
    
    friend bool operator==(BigNum const& lhs, BigNum const& rhs) {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }
    
    template <typename T> requires std::is_arithmetic_v<T>
    friend bool operator==(BigNum const& lhs, T rhs) {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }
    
    friend std::ostream& operator<<(std::ostream& ostream, BigNum const& number);
    
private:
    void canonicalize();
    
    template <typename T>
    bool representableAsImpl() const;
    
    bool isCanonical() const;
    
    long long toSigned() const;
    unsigned long long toUnsigned() const;
    double toDouble() const;
    
private:
    std::aligned_storage_t<4 * sizeof(void*), alignof(void*)> storage;
};

// MARK: Inline definitions

template <typename T> requires std::is_arithmetic_v<T>
inline BigNum::operator T() const {
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

std::strong_ordering operator<=>(BigNum const& lhs, std::signed_integral auto rhs) {
    return lhs <=> static_cast<long long>(rhs);
}

std::strong_ordering operator<=>(BigNum const& lhs, std::unsigned_integral auto rhs) {
    return lhs <=> static_cast<unsigned long long>(rhs);
}

} // namespace scatha

#endif // SCATHA_COMMON_BIGNUM_H_

