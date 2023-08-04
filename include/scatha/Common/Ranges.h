#ifndef SCATHA_COMMON_RANGES_H_
#define SCATHA_COMMON_RANGES_H_

#include <type_traits>

#include <range/v3/view.hpp>
#include <utl/type_traits.hpp>

namespace scatha {

template <typename T>
inline constexpr auto Filter =
    ranges::views::filter([](auto const& value) { return isa<T>(value); }) |
    ranges::views::transform([]<typename V>(V&& value) -> decltype(auto) {
        using NoRef = std::remove_reference_t<V>;
        using NoPtr = std::remove_pointer_t<NoRef>;
        if constexpr (std::is_pointer_v<NoRef>) {
            return cast<utl::copy_cv_t<NoPtr, T>*>(value);
        }
        else {
            return cast<utl::copy_cv_t<NoPtr, T>&>(value);
        }
    });

} // namespace scatha

#endif // SCATHA_COMMON_RANGES_H_
