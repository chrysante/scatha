#ifndef SVM_COMMON_H_
#define SVM_COMMON_H_

#include <cassert>
#include <cstdint>

namespace svm {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
static_assert(sizeof(f32) == 4);

using f64 = double;
static_assert(sizeof(f64) == 8);

[[noreturn]] inline void unreachable() {
    assert(false);
#if defined(__GNUC__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#endif
}

#if defined(__GNUC__)
#define SVM_NOINLINE      __attribute__((noinline))
#define SVM_ALWAYS_INLINE __attribute__((always_inline))
#define SVM_LIKELY(x)     __builtin_expect(!!(x), 1)
#define SVM_UNLIKELY(x)   __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
#define SVM_NOINLINE      __declspec(noinline)
#define SVM_ALWAYS_INLINE __forceinline
#define SVM_LIKELY(x)
#define SVM_UNLIKELY(x)
#else
#define SVM_NOINLINE
#define SVM_ALWAYS_INLINE
#define SVM_LIKELY(x)
#define SVM_UNLIKELY(x)
#endif

} // namespace svm

#endif // SVM_COMMON_H_
