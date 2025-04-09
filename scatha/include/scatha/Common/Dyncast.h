#ifndef SCATHA_COMMON_DYNCAST_H_
#define SCATHA_COMMON_DYNCAST_H_

#include <csp.hpp>

#include <scatha/Common/Base.h>

#define SC_DYNCAST_DEFINE(Type, ID, Parent, Corporeality)                      \
    CSP_DEFINE(Type, ID, Parent, Corporeality)

namespace scatha {

using namespace csp::ops;

} // namespace scatha

/// Convenience macro for 'match' expressions
#define SC_MATCH(...)                                                          \
    ::scatha::internal::Matcher(                                               \
        ::scatha::internal::Tag<csp::impl::DeduceReturnTypeTag>{},             \
        __VA_ARGS__)                                                           \
            ->*csp::overload

#define SC_MATCH_R(R, ...)                                                     \
    ::scatha::internal::Matcher(::scatha::internal::Tag<R>{}, __VA_ARGS__)     \
            ->*csp::overload

namespace scatha::internal {

template <typename>
struct Tag {};

template <typename R, typename... T>
struct Matcher {
    explicit Matcher(Tag<R>, T&... t): t(t...) {}

    std::tuple<T&...> t;
};

template <typename R, typename... T, typename... F>
SC_NODEBUG decltype(auto) operator->*(Matcher<R, T...> m,
                                      csp::overload<F...> const& f) {
    // clang-format off
    return std::apply([&](auto&&... t) -> decltype(auto) {
        return visit<R>(t..., f);
    }, m.t); // clang-format on
}

template <typename R, typename... T, typename... F>
SC_NODEBUG decltype(auto) operator->*(Matcher<R, T...> m,
                                      csp::overload<F...>&& f) {
    // clang-format off
    return std::apply([&](auto&&... t) -> decltype(auto) {
        return visit<R>(t..., std::move(f));
    }, m.t); // clang-format on
}

/// Special case for one argument where we avoid `std::tuple` and `std::apply`
template <typename R, typename T>
struct Matcher<R, T> {
    explicit Matcher(Tag<R>, T& t): t(t) {}

    T& t;
};

template <typename R, typename T, typename... F>
SC_NODEBUG decltype(auto) operator->*(Matcher<R, T> m,
                                      csp::overload<F...> const& f) {
    return visit<R>(m.t, f);
}

template <typename R, typename T, typename... F>
SC_NODEBUG decltype(auto) operator->*(Matcher<R, T> m,
                                      csp::overload<F...>&& f) {
    return visit<R>(m.t, f);
}

} // namespace scatha::internal

#endif // SCATHA_COMMON_DYNCAST_H_
