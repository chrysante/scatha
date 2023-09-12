#ifndef SCATHA_RUNTIME_COMMON_H_
#define SCATHA_RUNTIME_COMMON_H_

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace svm {

class VirtualMachine;

} // namespace svm

namespace scatha {

enum class BaseType : uint8_t {
    Void,
    Bool,
    Byte,
    S8,
    S16,
    S32,
    S64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,

    /// Aliases
    Int = S64,
    Float = F32,
    Double = F64,
};

enum class Qualifier : uint8_t {
    None,
    Ref,
    MutRef,
    ArrayRef,
    MutArrayRef,
};

struct QualType {
    QualType(BaseType base): QualType(base, Qualifier::None) {}

    QualType(BaseType base, Qualifier qual): base(base), qual(qual) {}

    BaseType base;
    Qualifier qual;
};

struct ForeignFunctionID {
    size_t slot, index;
};

template <typename T>
inline constexpr bool Trivial =
    std::is_trivially_copyable_v<T> || std::is_same_v<T, void>;

} // namespace scatha

namespace scatha::internal {

template <typename T>
T load(void const* ptr) {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    std::memcpy(&storage, ptr, sizeof(T));
    return reinterpret_cast<T const&>(storage);
}

template <typename T>
void store(void* dest, T const& t) {
    std::memcpy(dest, &t, sizeof(T));
}

size_t constexpr ceildiv(size_t p, size_t q) { return p / q + !!(p % q); }

} // namespace scatha::internal

#endif // SCATHA_RUNTIME_COMMON_H_
