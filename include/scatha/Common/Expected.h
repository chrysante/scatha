#ifndef SCATHA_COMMON_EXPECTED_H_
#define SCATHA_COMMON_EXPECTED_H_

#include <concepts>
#include <memory>

#include <utl/common.hpp>
#include <utl/expected.hpp>

#include <scatha/Common/Base.h>

namespace scatha {

namespace internal {

// clang-format off

/// Concept to restrict error construction from arbitrary arguments. To allow
/// error construction from arbitrary arguments, the expected type must not be
/// constructible from those arguments to avoid ambiguities.
template <typename T, typename E, typename... Args>
concept ErrorConstructibleFrom =
    std::constructible_from<E, Args...> &&
    !std::constructible_from<T, Args...>;

// clang-format on

} // namespace internal

template <typename T, typename E>
class Expected {
public:
    Expected(T const& value): _e(value) {}

    Expected(T&& value): _e(std::move(value)) {}

    template <typename... Args>
    requires std::constructible_from<T, Args...> Expected(Args&&... args):
        _e(T(std::forward<Args>(args)...)) {}

    Expected(E const& error): _e(utl::unexpected(error)) {}

    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}

    template <typename... Args>
    requires internal::ErrorConstructibleFrom<T, E, Args...> Expected(
        Args&&... args):
        _e(utl::unexpected(std::forward<Args>(args)...)) {}

    template <std::convertible_to<T> U>
    operator Expected<U, E>() const& {
        if (hasValue()) {
            return value();
        }
        else {
            return error();
        }
    }

    template <typename U>
    requires std::convertible_to<U&&, T&&>
    operator Expected<U, E>() && {
        if (hasValue()) {
            return std::move(value());
        }
        else {
            return std::move(error());
        }
    }

    bool hasValue() const { return _e.has_value(); }

    explicit operator bool() const { return hasValue(); }

    T& value() & { return getValueImpl(*this); }
    T&& value() && { return getValueImpl(std::move(*this)); }
    T const& value() const& { return getValueImpl(*this); }
    T const&& value() const&& { return getValueImpl(std::move(*this)); }

    template <typename U>
    decltype(auto) valueOr(U&& alternative) {
        return hasValue() ? *_e : std::forward<U>(alternative);
    }

    template <typename U>
    decltype(auto) valueOr(U&& alternative) const {
        return hasValue() ? *_e : std::forward<U>(alternative);
    }

    E& error() & { return getErrorImpl(*this); }
    E&& error() && { return getErrorImpl(std::move(*this)); }
    E const& error() const& { return getErrorImpl(*this); }
    E const&& error() const&& { return getErrorImpl(std::move(*this)); }

    T* operator->() { return _e.operator->(); }
    T const* operator->() const { return _e.operator->(); }

    T& operator*() & { return value(); }
    T&& operator*() && { return value(); }
    T const& operator*() const& { return value(); }
    T const&& operator*() const&& { return value(); }

private:
    template <typename Self>
    static decltype(auto) getValueImpl(Self&& self) {
        if (!self._e.has_value()) {
            throw std::move(self.error());
        }
        return *std::forward<Self>(self)._e;
    }

    template <typename Self>
    static decltype(auto) getErrorImpl(Self&& self) {
        SC_EXPECT(!self._e.has_value());
        return std::forward<Self>(self)._e.error();
    }

private:
    utl::expected<T, E> _e;
};

template <typename E>
class Expected<void, E> {
public:
    Expected() = default;

    Expected(E const& error): _e(utl::unexpected(error)) {}

    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}

    template <typename... Args>
    requires internal::ErrorConstructibleFrom<void, E, Args...> Expected(
        Args&&... args):
        _e(utl::unexpected(std::forward<Args>(args)...)) {}

    bool hasValue() const { return _e.has_value(); }

    explicit operator bool() const { return hasValue(); }

    void value() const {
        if (!hasValue()) {
            throw error();
        }
    }

    E& error() & { return getErrorImpl(*this); }
    E&& error() && { return getErrorImpl(std::move(*this)); }
    E const& error() const& { return getErrorImpl(*this); }
    E const&& error() const&& { return getErrorImpl(std::move(*this)); }

private:
    template <typename Self>
    static decltype(auto) getErrorImpl(Self&& self) {
        SC_EXPECT(!self._e.has_value());
        return std::forward<Self>(self)._e.error();
    }

private:
    utl::expected<void, E> _e;
};

template <typename T, typename E>
class Expected<T&, E> {
public:
    Expected(T& value): _e(&value) {}

    Expected(E const& error): _e(utl::unexpected(error)) {}

    Expected(E&& error): _e(utl::unexpected(std::move(error))) {}

    template <typename... Args>
    requires internal::ErrorConstructibleFrom<T, E, Args...> Expected(
        Args&&... args):
        _e(utl::unexpected(std::forward<Args>(args)...)) {}

    bool hasValue() const { return _e.has_value(); }

    explicit operator bool() const { return hasValue(); }

    T& value() const {
        if (!hasValue()) {
            throw error();
        }
        return **_e;
    }

    E& error() & { return getErrorImpl(*this); }
    E&& error() && { return getErrorImpl(std::move(*this)); }
    E const& error() const& { return getErrorImpl(*this); }
    E const&& error() const&& { return getErrorImpl(std::move(*this)); }

    T* operator->() const { return *_e.operator->(); }

    T& operator*() const { return value(); }

private:
    template <typename Self>
    static decltype(auto) getErrorImpl(Self&& self) {
        SC_EXPECT(!self._e.has_value());
        return std::forward<Self>(self)._e.error();
    }

private:
    utl::expected<T*, E> _e;
};

} // namespace scatha

#endif // SCATHA_COMMON_EXPECTED_H_
