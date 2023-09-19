#ifndef SCATHA_COMMON_DYNCAST_H_
#define SCATHA_COMMON_DYNCAST_H_

#include <utl/dyncast.hpp>
#include <utl/utility.hpp>

#define SC_DYNCAST_MAP(Type, EnumValue, Abstractness)                          \
    UTL_DYNCAST_MAP(Type, EnumValue)                                           \
    UTL_INVOKE_MACRO(UTL_DYNCAST_##Abstractness, Type)

namespace scatha {

using utl::cast;
using utl::cast_or_null;
using utl::dyncast;
using utl::dyncast_or_null;
using utl::isa;
using utl::isa_or_null;
using utl::visit;

} // namespace scatha

/// Convenience macro for 'match' expressions
#define SC_MATCH(...) ::scatha::internal::Matcher(__VA_ARGS__)->*utl::overload

namespace scatha::internal {

template <typename... T>
struct Matcher {
    template <typename... U>
    explicit Matcher(U&&... t): t(t...) {}

    std::tuple<T&...> t;
};

template <typename... T>
Matcher(T&&...) -> Matcher<std::remove_reference_t<T>...>;

template <typename... T, typename... F>
decltype(auto) operator->*(Matcher<T...> m, utl::overload<F...> const& f) {
    // clang-format off
    return std::apply([&](auto&&... t) -> decltype(auto) {
        return visit(t..., f);
    }, m.t); // clang-format on
}

template <typename... T, typename... F>
decltype(auto) operator->*(Matcher<T...> m, utl::overload<F...>&& f) {
    // clang-format off
    return std::apply([&](auto&&... t) -> decltype(auto) {
        return visit(t..., std::move(f));
    }, m.t); // clang-format on
}

} // namespace scatha::internal

#endif // SCATHA_COMMON_DYNCAST_H_
