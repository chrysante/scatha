// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_APINT_H_
#define SCATHA_COMMON_APINT_H_

#include <compare>
#include <concepts>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <type_traits>

#include <scatha/Basic/Basic.h>

namespace scatha {

class APInt;

SCATHA(API) APInt operator+(APInt const& lhs, APInt const& rhs);
SCATHA(API) APInt operator-(APInt const& lhs, APInt const& rhs);
SCATHA(API) APInt operator*(APInt const& lhs, APInt const& rhs);
SCATHA(API) APInt operator/(APInt const& lhs, APInt const& rhs);
SCATHA(API) APInt operator%(APInt const& lhs, APInt const& rhs);

SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, APInt const& rhs);
SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, long long rhs);
SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, unsigned long long rhs);
SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, double rhs);
SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, std::signed_integral auto rhs);
SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, std::unsigned_integral auto rhs);
SCATHA(API) std::strong_ordering operator<=>(APInt const& lhs, std::floating_point auto rhs);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, APInt const& number);

class SCATHA(API) APInt {
public:
    // MARK: Construction & Lifetime
    APInt();
    APInt(std::signed_integral auto value): APInt(static_cast<long long>(value)) {}
    APInt(long long value);
    APInt(std::unsigned_integral auto value): APInt(static_cast<unsigned long long>(value)) {}
    APInt(unsigned long long value);
    APInt(double value);

    APInt(APInt const& rhs);
    APInt(APInt&& rhs) noexcept;
    APInt& operator=(APInt const& rhs);
    APInt& operator=(APInt&& rhs) noexcept;
    ~APInt();

    template <typename T>
    requires std::is_arithmetic_v<T>
    explicit operator T() const { return to<T>(); }

    template <typename T>
    requires std::is_arithmetic_v<T>
    T to() const;
    
    /// Convert a string to APInt.
    ///
    /// \details Whitespaces are ignored.
    ///
    /// \param value String to convert.
    /// If \p value contains a dot \p '.' it must not have  a base-specifier prefix and will be parsed as a floating
    /// point value.
    ///
    /// \param base Must be either 0 or between 2 and 16. If \p base is 0, the leading characters are used for
    /// disambiguation: \p 0x or \p 0X for hex, \p 0b or \p 0B for binary, \p 0 for octal, or decimal otherwise.
    ///
    /// \returns The converted value if the conversion was successful. Empty optional otherwise.
    ///
    static std::optional<APInt> fromString(std::string_view value, int base = 0);

    // MARK: Arithmetic

    APInt& operator+=(APInt const& rhs) &;
    template <typename T>
    requires std::is_arithmetic_v<T>
    APInt& operator+=(T rhs) & {
        return *this += APInt(rhs);
    }

    APInt& operator-=(APInt const& rhs) &;
    template <typename T>
    requires std::is_arithmetic_v<T>
    APInt& operator-=(T rhs) & {
        return *this -= APInt(rhs);
    }

    APInt& operator*=(APInt const& rhs) &;
    template <typename T>
    requires std::is_arithmetic_v<T>
    APInt& operator*=(T rhs) & {
        return *this *= APInt(rhs);
    }

    APInt& operator/=(APInt const& rhs) &;
    template <typename T>
    requires std::is_arithmetic_v<T>
    APInt& operator/=(T rhs) & {
        return *this /= APInt(rhs);
    }
    
    APInt& operator%=(APInt const& rhs) &;
    template <typename T>
    requires std::is_arithmetic_v<T>
    APInt& operator%=(T rhs) & {
        return *this /= APInt(rhs);
    }

    friend APInt operator+(APInt const& lhs, APInt const& rhs);
    friend APInt operator-(APInt const& lhs, APInt const& rhs);
    friend APInt operator*(APInt const& lhs, APInt const& rhs);
    friend APInt operator/(APInt const& lhs, APInt const& rhs);
    friend APInt operator%(APInt const& lhs, APInt const& rhs);

    // MARK: Queries

    /// Query this number for lossless convertability to C++ arithmetic types.
    ///
    /// \returns \p true iff \p *this is losslessly convertible to \p T
    template <typename T>
    requires std::is_arithmetic_v<T> bool
    representableAs() const {
        return representableAsImpl<T>();
    }

    std::string toString() const;

    void* getImplementationPointer() { return &storage; }
    void const* getImplementationPointer() const { return &storage; }

    friend std::strong_ordering operator<=>(APInt const& lhs, APInt const& rhs);
    friend std::strong_ordering operator<=>(APInt const& lhs, long long rhs);
    friend std::strong_ordering operator<=>(APInt const& lhs, unsigned long long rhs);
    friend std::strong_ordering operator<=>(APInt const& lhs, double rhs);

    friend bool operator==(APInt const& lhs, APInt const& rhs) { return (lhs <=> rhs) == std::strong_ordering::equal; }

    template <typename T>
    requires std::is_arithmetic_v<T>
    friend bool operator==(APInt const& lhs, T rhs) {
        return (lhs <=> rhs) == std::strong_ordering::equal;
    }

    friend std::ostream& operator<<(std::ostream& ostream, APInt const& number);

    friend struct std::hash<APInt>;

private:
    template <typename T>
    bool representableAsImpl() const;

    long long toSigned() const;
    unsigned long long toUnsigned() const;
    double toDouble() const;

private:
    std::aligned_storage_t<2 * sizeof(void*), alignof(void*)> storage;
};

} // namespace scatha

template <>
struct std::hash<scatha::APInt> {
    std::size_t operator()(scatha::APInt const& value) const;
};

// MARK: Inline definitions

template <typename T>
requires std::is_arithmetic_v<T>
inline T scatha::APInt::to() const {
    if constexpr (std::integral<T>) {
        return static_cast<T>(toSigned());
    }
    else {
        static_assert(std::floating_point<T>);
        return static_cast<T>(toDouble());
    }
}

std::strong_ordering scatha::operator<=>(APInt const& lhs, std::signed_integral auto rhs) {
    return operator<=>(lhs, static_cast<long long>(rhs));
}

std::strong_ordering scatha::operator<=>(APInt const& lhs, std::unsigned_integral auto rhs) {
    return operator<=>(lhs, static_cast<unsigned long long>(rhs));
}

std::strong_ordering scatha::operator<=>(APInt const& lhs, std::floating_point auto rhs) {
    return operator<=>(lhs, static_cast<double>(rhs));
}

#endif // SCATHA_COMMON_APINT_H_
