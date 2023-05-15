#ifndef SCATHA_COMMON_BASE_H_
#define SCATHA_COMMON_BASE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef _MSC_VER
#include <exception>
#endif

/// We define this unconditionally for now, to have assertions in optimized
/// builds
#define SC_DEBUG

/// MARK: API Export

#if defined(__GNUC__)

#define SCATHA_API     __attribute__((visibility("default")))
#define SCATHA_TESTAPI __attribute__((visibility("default")))

#elif defined(_MSC_VER)
#if defined(SC_APIEXPORT)
#define SCATHA_API     __declspec(dllexport)
#define SCATHA_TESTAPI __declspec(dllexport)
#elif defined(SC_APIIMPORT)
#define SCATHA_API     __declspec(dllimport)
#define SCATHA_TESTAPI __declspec(dllimport)
#endif // APIIMPORT / APIEXPORT

#else  // Compiler specific
#error Unsupported compiler
#endif

// Disable UBSAN for certain integer shift operations. Maybe rethink this later.
#if defined(__clang__) && __clang_major__ >= 10
#define SC_DISABLE_UBSAN __attribute__((no_sanitize("undefined")))
#else
#define SC_DISABLE_UBSAN
#endif

/// MARK: Assertions

#if defined(__clang__)
#define SC_DEBUGFAIL_IMPL()  __builtin_trap()
#define SC_DEBUGBREAK_IMPL() __builtin_debugtrap()
#elif defined(__GNUC__)
#define SC_DEBUGFAIL_IMPL()  __builtin_trap()
#define SC_DEBUGBREAK_IMPL() __builtin_trap()
#elif defined(_MSC_VER)
#define SC_DEBUGFAIL_IMPL()  (__debugbreak(), std::terminate())
#define SC_DEBUGBREAK_IMPL() __debugbreak()
#else
#error Unsupported Compiler
#endif

/// # SC_PRETTY_FUNC
#if defined(__GNUC__)
#define SC_PRETTY_FUNC __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define SC_PRETTY_FUNC __FUNCSIG__
#else
#error Unsupported Compiler
#endif

/// # SC_UNREACHABLE
#if __GNUC__
#define _SC_UNREACHABLE_IMPL() __builtin_unreachable()
#else
#define _SC_UNREACHABLE_IMPL() ((void)0)
#endif

#ifdef SC_DEBUG
#define SC_UNREACHABLE(...)                                                    \
    (::scatha::internal::unreachable(__FILE__, __LINE__, SC_PRETTY_FUNC),      \
     SC_DEBUGFAIL_IMPL())

#else  // SC_DEBUG
#define SC_UNREACHABLE(...) _SC_UNREACHABLE_IMPL()
#endif // SC_DEBUG

/// # SC_UNIMPLEMENTED
#define SC_UNIMPLEMENTED()                                                     \
    (::scatha::internal::unimplemented(__FILE__, __LINE__, SC_PRETTY_FUNC),    \
     SC_DEBUGFAIL_IMPL())

/// # SC_DEBUGBREAK
#define SC_DEBUGBREAK() SC_DEBUGBREAK_IMPL()

/// # SC_ASSUME
#if defined(__clang__)
#define SC_ASSUME(COND) __builtin_assume(COND)
#else
#define SC_ASSUME(COND) ((void)0)
#endif

/// # SC_ASSERT
#ifdef SC_DEBUG
#define SC_ASSERT(COND, MSG)                                                   \
    ((COND) ? (void)0 :                                                        \
              (::scatha::internal::assertionFailure(__FILE__,                  \
                                                    __LINE__,                  \
                                                    SC_PRETTY_FUNC,            \
                                                    #COND,                     \
                                                    MSG),                      \
               SC_DEBUGFAIL_IMPL()))
#else  // SC_DEBUG
#define SC_ASSERT(COND, MSG) SC_ASSUME(COND)
#endif // SC_DEBUG

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
    std::array<u8, sizeof(T)> result;
    std::memcpy(result.data(), &t, sizeof t);
    return result;
}

inline constexpr size_t invalidIndex = static_cast<size_t>(-1);

} // namespace scatha

namespace scatha::internal {

void SCATHA_API unimplemented(char const* file, int line, char const* function);

void SCATHA_API unreachable(char const* file, int line, char const* function);

void SCATHA_API assertionFailure(char const* file,
                                 int line,
                                 char const* function,
                                 char const* expr,
                                 char const* msg);

} // namespace scatha::internal

#endif // SCATHA_COMMON_BASE_H_
