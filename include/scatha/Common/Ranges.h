#ifndef SCATHA_COMMON_RANGES_H_
#define SCATHA_COMMON_RANGES_H_

#include <type_traits>

#include <range/v3/view.hpp>
#include <utl/type_traits.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Dyncast.h>

namespace scatha {

template <typename T>
inline constexpr auto Filter = ranges::views::filter(
                                   []<typename V>(V const& value) {
    using NoRef = std::remove_reference_t<V>;
    if constexpr (std::is_pointer_v<NoRef>) {
        return isa_or_null<T>(value);
    }
    else {
        return isa<T>(value);
    }
}) | ranges::views::transform([]<typename V>(V&& value) -> decltype(auto) {
    using NoRef = std::remove_reference_t<V>;
    using NoPtr = std::remove_pointer_t<NoRef>;
    if constexpr (std::is_pointer_v<NoRef>) {
        return cast<utl::copy_cv_t<NoPtr, T>*>(value);
    }
    else {
        return cast<utl::copy_cv_t<NoPtr, T>&>(value);
    }
});

/// Range casting functions
#define SC_CASTING_RANGE_DEF(Name, FuncName)                                   \
    template <typename Target>                                                 \
    inline constexpr auto Name =                                               \
        ranges::views::transform([]<typename T>(T&& p) -> decltype(auto) {     \
            return FuncName<Target>(std::forward<T>(p));                       \
        })

SC_CASTING_RANGE_DEF(Cast, cast);
SC_CASTING_RANGE_DEF(CastOrNull, cast_or_null);
SC_CASTING_RANGE_DEF(DynCast, dyncast);
SC_CASTING_RANGE_DEF(DynCastOrNull, dyncast_or_null);

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

} // namespace scatha

#endif // SCATHA_COMMON_RANGES_H_
