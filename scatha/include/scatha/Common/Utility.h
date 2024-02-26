#ifndef SCATHA_COMMON_UTILITY_H_
#define SCATHA_COMMON_UTILITY_H_

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <range/v3/view.hpp>

namespace scatha {

/// Reinterpret the bytes of \p t as a `std::array` of bytes.
template <typename T>
    requires std::is_standard_layout_v<T>
std::array<uint8_t, sizeof(T)> decompose(T const& t) {
    std::array<uint8_t, sizeof(T)> result;
    std::memcpy(result.data(), &t, sizeof t);
    return result;
}

/// # EnumSize

namespace internal {

template <typename E>
struct EnumSizeImpl;

template <typename E>
    requires requires { E::COUNT; }
struct EnumSizeImpl<E>:
    std::integral_constant<size_t, static_cast<size_t>(E::COUNT)> {};

template <typename E>
    requires(!requires { E::COUNT; }) && requires { E::LAST; }
struct EnumSizeImpl<E>:
    std::integral_constant<size_t, static_cast<size_t>(E::LAST) + 1> {};

} // namespace internal

/// Convenient accessor for `static_cast<size_t>(Enum::COUNT)` or
/// `static_cast<size_t>(Enum::LAST) + 1` depending on which is well formed
template <typename E>
    requires std::is_enum_v<E>
inline constexpr size_t EnumSize = internal::EnumSizeImpl<E>::value;

/// View over all values in an enum
/// \Warning Requires the enum cases to start at value 0, have no holes and end
/// at `EnumSize<E>`
template <typename E>
    requires std::is_enum_v<E>
inline auto EnumRange() {
    return ranges::views::iota(size_t{ 0 }, EnumSize<E>) |
           ranges::views::transform(
               [](size_t value) { return static_cast<E>(value); });
}

/// # impl_cast

namespace internal {

template <typename To>
struct ImplCastFn {
    template <typename From>
        requires std::is_pointer_v<To> && std::is_convertible_v<From*, To>
    constexpr To operator()(From* p) const {
        return p;
    }

    template <typename From>
        requires std::is_reference_v<To> && std::is_convertible_v<From&, To>
    constexpr To operator()(From& p) const {
        return p;
    }
};

} // namespace internal

template <typename To>
inline constexpr internal::ImplCastFn<To> impl_cast{};

} // namespace scatha

#endif // SCATHA_COMMON_UTILITY_H_
