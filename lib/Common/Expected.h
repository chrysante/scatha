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
		template <std::derived_from<E> U>
		Expected(U&& error): _e(utl::unexpected(std::make_unique<std::decay_t<U>>(UTL_FORWARD(error)))) {}
		
		bool hasValue() const { return _e.has_value(); }
		explicit operator bool() const { return hasValue(); }
		
		T& value() { return *_e; }
		T const& value() const { return *_e; }
		
		E& error() { return *_e.error(); }
		E const& error() const { return *_e.error(); }
		
		T* operator->() { return _e.operator->(); }
		T const* operator->() const { return _e.operator->(); }
		
		T& operator*() { return _e.operator*(); }
		T const& operator*() const { return _e.operator*(); }
		
	private:
		utl::expected<T, std::unique_ptr<E>> _e;
	};
	
	template <typename T, typename E>
	class Expected<T&, E> {
	public:
		Expected(T& value): _e(&value) {}
		template <std::derived_from<E> U>
		Expected(U&& error): _e(utl::unexpected(std::make_unique<std::decay_t<U>>(UTL_FORWARD(error)))) {}
		
		bool hasValue() const { return _e.has_value(); }
		explicit operator bool() const { return hasValue(); }
		
		T& value() { return **_e; }
		T const& value() const { return **_e; }
		
		E& error() { return *_e.error(); }
		E const& error() const { return *_e.error(); }
		
		T* operator->() { return *_e.operator->(); }
		T const* operator->() const { return *_e.operator->(); }
		
		T& operator*() { return *_e.operator*(); }
		T const& operator*() const { return *_e.operator*(); }
		
	private:
		utl::expected<T*, std::unique_ptr<E>> _e;
	};
	
}

#endif // SCATHA_COMMON_EXPECTED_H_

