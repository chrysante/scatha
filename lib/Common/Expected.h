#ifndef SCATHA_COMMON_EXPECTED_H_
#define SCATHA_COMMON_EXPECTED_H_

#include <concepts>
#include <memory>

#include <utl/common.hpp>
#include <utl/expected.hpp>

namespace scatha {

namespace internal {

/// Concept to restrict error construction from arbitrary arguments. To allow error construction from arbitrary
/// arguments, the expected type must not be constructible from those arguments to avoid ambiguities.
template <typename T, typename E, typename... Args>
concept ErrorConstructibleFrom = std::constructible_from<E, Args...> && !std::constructible_from<T, Args...>;

}

template <typename T, typename E>
class Expected {
public:
    Expected(T const& value): _e(value) {}
    
    Expected(T&& value): _e(std::move(value)) {}
    
    template <typename... Args> requires std::constructible_from<T, Args...>
    Expected(Args&&... args): _e(T(std::forward<Args>(args)...)) {}
    
    Expected(E const& error): _e(utl::unexpected(error)) {}
    
    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}
    
    template <typename... Args> requires internal::ErrorConstructibleFrom<T, E, Args...>
    Expected(Args&&... args): _e(utl::unexpected(std::forward<Args>(args)...)) {}
    
    template <std::convertible_to<T> U>
    operator Expected<U, E>() const& {
        if (hasValue()) { return value(); }
        else { return error(); }
    }
    
    template <typename U> requires std::convertible_to<U&&, T&&> 
    operator Expected<U, E>()&& {
        if (hasValue()) { return std::move(value()); }
        else { return std::move(error()); }
    }
    
    bool hasValue() const { return _e.has_value(); }
    
    explicit operator bool() const { return hasValue(); }

    T& value() { return *_e; }
    T const& value() const { return *_e; }

    E& error() { return _e.error(); }
    E const& error() const { return _e.error(); }

    T* operator->() { return _e.operator->(); }
    T const* operator->() const { return _e.operator->(); }

    T& operator*() { return _e.operator*(); }
    T const& operator*() const { return _e.operator*(); }

private:
    utl::expected<T, E> _e;
};

template <typename E>
class Expected<void, E> {
public:
    Expected() = default;
    
    Expected(E const& error): _e(utl::unexpected(error)) {}
    
    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}
    
    template <typename... Args> requires internal::ErrorConstructibleFrom<void, E, Args...>
    Expected(Args&&... args): _e(utl::unexpected(std::forward<Args>(args)...)) {}
    
    bool hasValue() const { return _e.has_value(); }
    
    explicit operator bool() const { return hasValue(); }

    void value() const {}

    E& error() { return _e.error(); }
    E const& error() const { return _e.error(); }

private:
    utl::expected<void, E> _e;
};

template <typename T, typename E>
class Expected<T&, E> {
public:
    Expected(T& value): _e(&value) {}
    
    Expected(E const& error): _e(utl::unexpected(error)) {}
    
    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}
    
    template <typename... Args> requires internal::ErrorConstructibleFrom<T, E, Args...>
    Expected(Args&&... args): _e(utl::unexpected(std::forward<Args>(args)...)) {}
    
    bool hasValue() const { return _e.has_value(); }
    
    explicit operator bool() const { return hasValue(); }

    T& value() { return **_e; }
    T const& value() const { return **_e; }

    E& error() { return _e.error(); }
    E const& error() const { return _e.error(); }

    T* operator->() { return *_e.operator->(); }
    T const* operator->() const { return *_e.operator->(); }

    T& operator*() { return *_e.operator*(); }
    T const& operator*() const { return *_e.operator*(); }

private:
    utl::expected<T*, E> _e;
};

} // namespace scatha

#endif // SCATHA_COMMON_EXPECTED_H_
