#ifndef SCATHA_COMMON_BASE_H_
#define SCATHA_COMMON_BASE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <type_traits>

#ifndef SC_DEBUG
#ifdef NDEBUG
#define SC_DEBUG 0
#else
#define SC_DEBUG 1
#endif // NDEBUG
#endif // SC_DEBUG

/// MARK: API Export

#if defined(__GNUC__)
#define SCATHA_API __attribute__((visibility("default")))
#define SCTEST_API __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#if defined(SC_APIEXPORT)
#define SCATHA_API __declspec(dllexport)
#define SCTEST_API __declspec(dllexport)
#elif defined(SC_APIIMPORT)
#define SCATHA_API __declspec(dllimport)
#define SCTEST_API __declspec(dllimport)
#elif
#error Need either SC_APIEXPORT or SC_APIIMPORT defined
#endif // APIIMPORT / APIEXPORT

#else // Compiler specific
#error Unsupported compiler
#endif

/// Disable UBSAN for certain integer shift operations. Maybe rethink this
/// later.
#if defined(__clang__) && __clang_major__ >= 10
#define SC_DISABLE_UBSAN __attribute__((no_sanitize("undefined")))
#else
#define SC_DISABLE_UBSAN
#endif

/// Helper macro to declare move operations as defaulted and copy operations as
/// deleted. This is useful for public interface classes, because for classes
/// marked `dllexport`, MSVC tries to instantiate copy operations unless they
/// are explicitly deleted.
#define SC_MOVEONLY(Type)                                                      \
    Type(Type const&) = delete;                                                \
    Type& operator=(Type const&) = delete;                                     \
    Type(Type&&) noexcept = default;                                           \
    Type& operator=(Type&&) noexcept = default

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
#if defined(__GNUC__)
#define _SC_UNREACHABLE_IMPL() __builtin_unreachable()
#elif defined(_MSC_VER)
#define _SC_UNREACHABLE_IMPL() __assume(false)
#else
#define _SC_UNREACHABLE_IMPL() ((void)0)
#endif

#if SC_DEBUG
#define SC_UNREACHABLE(...)                                                    \
    (::scatha::internal::unreachable(__FILE__, __LINE__, SC_PRETTY_FUNC),      \
     ::scatha::internal::handleAssertFailure())

#else // SC_DEBUG
#define SC_UNREACHABLE(...) _SC_UNREACHABLE_IMPL()
#endif // SC_DEBUG

/// # SC_UNIMPLEMENTED
#define SC_UNIMPLEMENTED()                                                     \
    (::scatha::internal::unimplemented(__FILE__, __LINE__, SC_PRETTY_FUNC),    \
     ::scatha::internal::handleAssertFailure())

/// # SC_DEBUGFAIL
#define SC_DEBUGFAIL() SC_DEBUGFAIL_IMPL()

/// # SC_DEBUGBREAK
#define SC_DEBUGBREAK() SC_DEBUGBREAK_IMPL()

/// # SC_ASSERT
#if SC_DEBUG
#define SC_ASSERT(COND, MSG)                                                   \
    ((COND) ? (void)0 :                                                        \
              (::scatha::internal::assertionFailure(__FILE__,                  \
                                                    __LINE__,                  \
                                                    SC_PRETTY_FUNC,            \
                                                    #COND,                     \
                                                    MSG),                      \
               ::scatha::internal::handleAssertFailure()))
#else // SC_DEBUG
#define SC_ASSERT(COND, MSG) (void)sizeof((bool)(COND))
#endif // SC_DEBUG

/// # SC_EXPECT
#define SC_EXPECT(COND) SC_ASSERT(COND, "Precondition failed")

/// # SC_RELASSERT
#define SC_RELASSERT(COND, MSG)                                                \
    ((COND) ? (void)0 :                                                        \
              (::scatha::internal::assertionFailure(__FILE__,                  \
                                                    __LINE__,                  \
                                                    SC_PRETTY_FUNC,            \
                                                    #COND,                     \
                                                    MSG),                      \
               ::scatha::internal::relfail()))

#define SC_ABORT() ::scatha::internal::relfail()

/// Unicode symbols in terminal output
#if defined(__APPLE__) /// MacOS supports unicode symbols in terminal
#define SC_UNICODE_TERMINAL 1
#endif

/// # SC_CONCAT
#define SC_CONCAT(a, b)      SC_CONCAT_IMPL(a, b)
#define SC_CONCAT_IMPL(a, b) a##b

/// # SC_NODEBUG
/// We annotate simple one line getter functions with this attribute macro. This
/// way if we try to step into the function `codegen()` in the following example
/// and `LHS()` is annotated `NODEBUG`, we don't jump to the source code of
/// `LHS()` first but directly into `codegen()`
///
///     codegen(expr->LHS());
///
#if defined(__GNUC__)
// However we disable this for now because it disables code execution in the
// debugger...
#define SC_NODEBUG __attribute__((nodebug))
#else
#define SC_NODEBUG
#endif

namespace scatha {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
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

/// Convenient accessor for `static_cast<size_t>(Enum::COUNT)`
template <typename E>
    requires std::is_enum_v<E>
inline constexpr size_t EnumSize = static_cast<size_t>(E::COUNT);

#define SC_ENUM_SIZE_DEF(Enum, Size)                                           \
    template <>                                                                \
    inline constexpr size_t scatha::EnumSize<Enum> = Size;

#define SC_ENUM_SIZE_LAST_DEF(Enum, Last)                                      \
    template <>                                                                \
    inline constexpr size_t scatha::EnumSize<Enum> =                           \
        static_cast<size_t>(Enum::Last) + 1;

/// # impl_cast

namespace internal {

template <typename To>
struct ImplCastFn {
    template <typename From>
        requires std::is_pointer_v<To> && std::is_convertible_v<From*, To>
    constexpr To operator()(From* p) const {
        return p;
    }

    template <typename From>
        requires std::is_reference_v<To> && std::is_convertible_v<From&, To>
    constexpr To operator()(From& p) const {
        return p;
    }
};

} // namespace internal

template <typename To>
inline constexpr internal::ImplCastFn<To> impl_cast{};

} // namespace scatha

namespace scatha::internal {

void SCATHA_API unimplemented(char const* file, int line, char const* function);

void SCATHA_API unreachable(char const* file, int line, char const* function);

void SCATHA_API assertionFailure(char const* file,
                                 int line,
                                 char const* function,
                                 char const* expr,
                                 char const* msg);

/// Calls `std::abort()`
[[noreturn]] void SCATHA_API relfail();

/// Calls `std::abort()`
[[noreturn]] void SCATHA_API doAbort();

/// Exception class to be thrown if the installed assertion handler is `Throw`
struct SCATHA_API AssertionFailure: std::exception {
    char const* what() const noexcept override { return "Failed assertion"; }
};

/// Different ways to handle assertion failures
enum class AssertFailureHandler { Break, Abort, Throw };

/// Retrieve the way to handle assertion failures set by the environment
SCATHA_API AssertFailureHandler getAssertFailureHandler();

[[noreturn]]
#if defined(__GNUC__)
__attribute__((always_inline, nodebug))
#elif defined(_MSC_VER)
__forceinline
#endif
inline void
    handleAssertFailure() {
    using enum AssertFailureHandler;
    switch (getAssertFailureHandler()) {
    case Break:
        /// We use `SC_DEBUGFAIL` here instead of `SC_DEBUGBREAK` because this
        /// function must be annotated with `noreturn` as it is used to handle
        /// unreachable code. If we want to use `SC_DEBUGBREAK` we must provide
        /// seperate handlers for assertions and unreachables
        SC_DEBUGFAIL();
    case Abort:
        doAbort();
    case Throw:
        throw AssertionFailure();
    }
}

} // namespace scatha::internal

#endif // SCATHA_COMMON_BASE_H_
