#ifndef SVM_VIRTUALPOINTER_H_
#define SVM_VIRTUALPOINTER_H_

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace svm {

/// A virtual memory pointer
struct VirtualPointer {
    /// Null pointer constant
    static VirtualPointer const Null;

    /// Increments the pointer by \p offset bytes.
    /// \Returns a reference to `*this`
    VirtualPointer& operator+=(std::integral auto offset) {
        this->offset += static_cast<uint64_t>(offset);
        return *this;
    }

    /// Decrements the pointer by \p offset bytes.
    /// \Returns a reference to `*this`
    VirtualPointer& operator-=(std::integral auto offset) {
        this->offset -= static_cast<uint64_t>(offset);
        return *this;
    }

    bool operator==(VirtualPointer const&) const = default;

    /// The offset into the memory slot this pointer points to
    uint64_t offset : 48;

    /// The index of the memory slot this pointer points to
    uint64_t slotIndex : 16;
};

inline VirtualPointer const VirtualPointer::Null{};

/// Print \p ptr to \p ostream
std::ostream& operator<<(std::ostream& ostream, VirtualPointer ptr);

/// \Returns `ptr + offset`
inline VirtualPointer operator+(VirtualPointer ptr, std::integral auto offset) {
    ptr += offset;
    return ptr;
}

/// \Returns `ptr - offset`
inline VirtualPointer operator-(VirtualPointer ptr, std::integral auto offset) {
    ptr -= offset;
    return ptr;
}

/// \Returns `p - q`
inline ptrdiff_t operator-(VirtualPointer p, VirtualPointer q) {
    return static_cast<ptrdiff_t>(p.offset) - static_cast<ptrdiff_t>(q.offset);
}

/// \Returns `true` if \p ptr is aligned to a boundary of \p align bytes
inline bool isAligned(VirtualPointer ptr, size_t align) {
    return ptr.offset % align == 0;
}

/// Adds the smallest number of bytes to \p ptr such that \p ptr is aligned to a
/// boundary of \p align bytes
inline void alignTo(VirtualPointer& ptr, size_t align) {
    assert(std::popcount(align) == 1 && "Not a power of two");
    if (!isAligned(ptr, align)) {
        ptr.offset += align - ptr.offset % align;
    }
}

static_assert(sizeof(VirtualPointer) == 8,
              "Pointers must be exactly 8 bytes in size because we expose them "
              "as uint64_t");

} // namespace svm

#endif // SVM_VIRTUALPOINTER_H_
