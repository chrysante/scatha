#pragma once

#ifndef SCATHA_BASIC_COMMON_H_
#define SCATHA_BASIC_COMMON_H_

#include <cstdint>
#include <cstddef>

#define SCATHA(Name, ...) _SCATHA_PD_##Name(__VA_ARGS__)

#define _SCATHA_PD_PURE() __attribute__((const))

#if defined(__clang__) && __clang_major__ >= 10
#	define _SCATHA_PD_DISABLE_UBSAN() __attribute__((no_sanitize("undefined")))
#else
#	define _SCATHA_PD_DISABLE_UBSAN() 
#endif

/// MARK: Assertions
#define SC_DEBUGFAIL() __builtin_trap()
#define SC_DEBUGBREAK() __builtin_debugtrap()

// make this compiler intrinsic unreachable
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
	
}


#endif // SCATHA_BASIC_COMMON_H_
