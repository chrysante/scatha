#ifndef SCATHA_COMMON_RANGES_H_
#define SCATHA_COMMON_RANGES_H_

#include <tuple>
#include <type_traits>

#include <range/v3/view.hpp>
#include <utl/type_traits.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha {

template <typename... T>
    requires(sizeof...(T) > 0)
inline constexpr auto Filter = ranges::views::filter(
                                   []<typename V>(V const& value) {
    using NoRef = std::remove_reference_t<V>;
    if constexpr (std::is_pointer_v<NoRef>) {
        return (... || isa<T>(value));
    }
    else {
        return (... || isa<T>(value));
    }
}) | ranges::views::transform([]<typename V>(V&& value) -> decltype(auto) {
    if constexpr (sizeof...(T) > 1) {
        return std::forward<V>(value);
    }
    else {
        using NoRef = std::remove_reference_t<V>;
        using NoPtr = std::remove_pointer_t<NoRef>;
        using First = std::tuple_element_t<0, std::tuple<T...>>;
        if constexpr (std::is_pointer_v<NoRef>) {
            return cast<utl::copy_cv_t<NoPtr, First>*>(value);
        }
        else {
            return cast<utl::copy_cv_t<NoPtr, First>&>(value);
        }
    }
});

/// View applying `std::to_address` to every element
inline constexpr auto ToAddress = ranges::views::transform(
    []<typename T>(T&& p) { return std::to_address(std::forward<T>(p)); });

/// View applying `std::to_address` to every element and converting the pointer
/// to const
inline constexpr auto ToConstAddress =
    ranges::views::transform([]<typename T>(T&& p) -> auto const* {
        return std::to_address(std::forward<T>(p));
    });

/// View applying `operator&` to every element
inline constexpr auto TakeAddress = ranges::views::transform(
    []<typename T>(T&& t) -> decltype(auto) { return &std::forward<T>(t); });

/// View applying `operator*` to every element
inline constexpr auto Dereference = ranges::views::transform(
    []<typename T>(T&& t) -> decltype(auto) { return *std::forward<T>(t); });

/// Filter view to ignore null pointers
inline constexpr auto NonNull =
    ranges::views::filter([](auto const& ptr) { return ptr != nullptr; });

/// Turn any range into an opaquely typed range
inline constexpr auto Opaque =
    ranges::views::transform([]<typename T>(T&& value) -> decltype(auto) {
        return std::forward<T>(value);
    });

/// Shorthand for `ranges::to<utl::small_vector<...>>` to make it less verbose
template <size_t Size>
struct ToSmallVectorFn {};

template <size_t Size = ~size_t{}>
inline constexpr ToSmallVectorFn<Size> ToSmallVector{};

template <ranges::range R, size_t Size>
auto operator|(R&& r, ToSmallVectorFn<Size>) {
    using T = ranges::range_value_t<R>;
    if constexpr (Size == ~size_t{}) {
        return std::forward<R>(r) | ranges::to<utl::small_vector<T>>;
    }
    else {
        return std::forward<R>(r) | ranges::to<utl::small_vector<T, Size>>;
    }
}

template <typename E>
    requires std::is_enum_v<E>
inline auto EnumRange() {
    return ranges::views::iota(size_t{ 0 }, EnumSize<E>) |
           ranges::views::transform(
               [](size_t value) { return static_cast<E>(value); });
}

} // namespace scatha

#endif // SCATHA_COMMON_RANGES_H_
