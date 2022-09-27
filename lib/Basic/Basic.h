#pragma once

#ifndef SCATHA_BASIC_COMMON_H_
#define SCATHA_BASIC_COMMON_H_

#include <array>
#include <cstdint>
#include <cstddef>
#include <type_traits>

#ifdef _MSC_VER
#include <exception>
#endif

#include <utl/bit.hpp>

#define SCATHA(Name, ...) _SCATHA_PD_##Name(__VA_ARGS__)

#if defined(__GNUC__)
#define _SCATHA_PD_PURE() __attribute__((pure))
#define _SCATHA_PD_CONST() __attribute__((const))
#else
#define _SCATHA_PD_PURE() 
#define _SCATHA_PD_CONST() 
#define _SCATHA_PD_API() __declspec(dllexport) 
#define _SCATHA_PD_MOVE_ONLY(TYPE) \
	TYPE() = default;              \
	TYPE(TYPE&&) = default;        \
	TYPE(TYPE const&) = delete 
#endif

#if defined(__clang__) && __clang_major__ >= 10
#	define _SCATHA_PD_DISABLE_UBSAN() __attribute__((no_sanitize("undefined")))
#else
#	define _SCATHA_PD_DISABLE_UBSAN() 
#endif

/// MARK: Assertions
#if defined(__clang) || defined(__GNUC__)
#	define SC_DEBUGFAIL() __builtin_trap()
#	define SC_DEBUGBREAK() __builtin_debugtrap()
#elif defined(_MSC_VER)
#	define SC_DEBUGFAIL() (__debugbreak(), std::terminate())
#	define SC_DEBUGBREAK() __debugbreak()
#else
#	error Unsupported Compiler
#endif

// TODO: Make this compiler intrinsic unreachable
#define SC_UNREACHABLE() SC_DEBUGFAIL()

#define SC_ASSERT(COND, MSG) ((COND) ? (void)0 : SC_DEBUGFAIL())

#define SC_ASSERT_AUDIT(COND, MSG) SC_ASSERT(COND, MSG)

#define SC_EXPECT(COND, MSG) SC_ASSERT(COND, MSG)

#define SC_EXPECT_AUDIT(COND, MSG) SC_ASSERT(COND, MSG)

#define SC_NO_DEFAULT_CASE() default: SC_DEBUGFAIL()

namespace scatha {
	
	using i8  = std::int8_t;
	using i16 = std::int16_t;
	using i32 = std::int32_t;
	using i64 = std::int64_t;

	using u8  = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;
	
	using f32 = float;
	using f64 = double;
	
	static_assert(sizeof(f32) == 4);
	static_assert(sizeof(f64) == 8);

	// Reinterpret the bytes of t as a std::array of bytes
	template <typename T> requires std::is_standard_layout_v<T>
	std::array<u8, sizeof(T)> decompose(T const& t) {
		return utl::bit_cast<std::array<u8, sizeof(T)>>(t);
	}
	
}


#endif // SCATHA_BASIC_COMMON_H_
