#ifndef SCATHA_COMMON_MATCH_H_
#define SCATHA_COMMON_MATCH_H_

#include <utl/utility.hpp>

#include "Common/Dyncast.h"

#define SC_MATCH(arg) ::scatha::internal::Matcher(arg)->*utl::overload

namespace scatha::internal {

template <typename T>
struct Matcher {
    explicit Matcher(T& t): t(t) {}

    T& t;
};

template <typename T>
Matcher(T const&) -> Matcher<T const>;

template <typename T, typename... F>
auto operator->*(Matcher<T> m, utl::overload<F...> const& f) -> decltype(auto) {
    return visit(m.t, f);
}

template <typename T, typename... F>
auto operator->*(Matcher<T> m, utl::overload<F...>&& f) -> decltype(auto) {
    return visit(m.t, std::move(f));
}

} // namespace scatha::internal

#endif // SCATHA_COMMON_MATCH_H_
