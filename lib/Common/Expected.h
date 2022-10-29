#ifndef SCATHA_COMMON_EXPECTED_H_
#define SCATHA_COMMON_EXPECTED_H_

#include <concepts>
#include <memory>

#include <utl/common.hpp>
#include <utl/expected.hpp>

namespace scatha {

template <typename T, typename E>
class Expected {
public:
    Expected(T const& value): _e(value) {}
    Expected(T&& value): _e(std::move(value)) {}
    Expected(E const& error): _e(utl::unexpected(error)) {}
    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}

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
