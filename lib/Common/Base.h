#ifndef SCATHA_COMMON_BASE_H_
#define SCATHA_COMMON_BASE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef _MSC_VER
#include <exception>
#endif

#include <utl/bit.hpp>

#define SCATHA(Name, ...) _SCATHA_PD_##Name(__VA_ARGS__)

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
// POSIX Platform
#define _SCATHA_PD_POSIX()   1
#define _SCATHA_PD_WINDOWS() 0
#elif defined(MSC_VER)
// Windows Platform
#define _SCATHA_PD_POSIX()   0
#define _SCATHA_PD_WINDOWS() 1
#endif

#define _SCATHA_PD_DEBUG() 1

#if defined(__GNUC__)
#define _SCATHA_PD_GNU() 1
#else
#define _SCATHA_PD_GNU() 0
#endif

#if defined(__clang__)
#define _SCATHA_PD_CLANG() 1
#else
#define _SCATHA_PD_CLANG() 0
#endif

#if SCATHA(GNU)
#define _SCATHA_PD_PURE()  __attribute__((pure))
#define _SCATHA_PD_CONST() __attribute__((const))
#else
#define _SCATHA_PD_PURE()
#define _SCATHA_PD_CONST()
#endif

#define _SCATHA_PD_DEBUG() 1

// API Export
#if SCATHA(GNU)
#define _SCATHA_PD_API() __attribute__((visibility("default")))
#elif SCATHA(WINDOWS)
#define _SCATHA_PD_API() __declspec(dllexport)
#endif

#define SCATHA_API     _SCATHA_PD_API()
#define SCATHA_TESTAPI /// Can be removed once it's removed from all interfaces

// Disable UBSAN for certain integer shift operations. Maybe rethink this later.
#if defined(__clang__) && __clang_major__ >= 10
#define _SCATHA_PD_DISABLE_UBSAN() __attribute__((no_sanitize("undefined")))
#else
#define _SCATHA_PD_DISABLE_UBSAN()
#endif

/// MARK: Assertions
#if defined(__clang__)
#define SC_UNIMPLEMENTED() __builtin_trap()
#define SC_DEBUGBREAK()    __builtin_debugtrap()
#elif defined(__GNUC__)
#define SC_UNIMPLEMENTED() __builtin_trap()
#define SC_DEBUGBREAK()    __builtin_trap()
#elif defined(_MSC_VER)
#define SC_UNIMPLEMENTED() (__debugbreak(), std::terminate())
#define SC_DEBUGBREAK()    __debugbreak()
#else
#error Unsupported Compiler
#endif

// SC_UNREACHABLE
#if SCATHA(GNU)
#define _SC_UNREACHABLE_IMPL() __builtin_unreachable()
#else
#define _SC_UNREACHABLE_IMPL() ((void)0)
#endif

#if SCATHA(DEBUG)
#define SC_UNREACHABLE(...) SC_UNIMPLEMENTED()
#else
#define SC_UNREACHABLE(...) _SC_UNREACHABLE_IMPL()
#endif

// SC_ASSUME
#if SCATHA(CLANG)
#define SC_ASSUME(COND) __builtin_assume(COND)
#else
#define SC_ASSUME(COND) ((void)0)
#endif

// SC_ASSERT
#if SCATHA(DEBUG)
#define SC_ASSERT(COND, MSG) ((COND) ? (void)0 : SC_UNIMPLEMENTED())
#else
#define SC_ASSERT(COND, MSG) SC_ASSUME(COND)
#endif

// SC_ASSERT_AUDIT
#define SC_ASSERT_AUDIT(COND, MSG) SC_ASSERT(COND, MSG)

// SC_EXPECT
#define SC_EXPECT(COND, MSG) SC_ASSERT(COND, MSG)

// SC_EXPECT_AUDIT
#define SC_EXPECT_AUDIT(COND, MSG) SC_ASSERT(COND, MSG)

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
static_assert(sizeof(f32) == 4);

using f64 = double;
static_assert(sizeof(f64) == 8);

using std::size_t;
using ssize_t = std::ptrdiff_t;

/// Reinterpret the bytes of \p t as a `std::array` of bytes.
template <typename T>
    requires std::is_standard_layout_v<T>
std::array<u8, sizeof(T)> decompose(T const& t) {
    return utl::bit_cast<std::array<u8, sizeof(T)>>(t);
}

inline constexpr size_t invalidIndex = static_cast<size_t>(-1);

} // namespace scatha

#endif // SCATHA_COMMON_BASE_H_
