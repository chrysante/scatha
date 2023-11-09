#ifndef SVM_VIRTUALPOINTER_H_
#define SVM_VIRTUALPOINTER_H_

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace svm {

/// A virtual memory pointer
struct VirtualPointer {
    VirtualPointer& operator+=(std::integral auto offset) {
        this->offset += static_cast<uint64_t>(offset);
        return *this;
    }

    VirtualPointer& operator-=(std::integral auto offset) {
        this->offset -= static_cast<uint64_t>(offset);
        return *this;
    }

    uint64_t offset    : 48;
    uint64_t slotIndex : 16;
};

inline VirtualPointer operator+(VirtualPointer p, std::integral auto offset) {
    p += offset;
    return p;
}

inline VirtualPointer operator-(VirtualPointer p, std::integral auto offset) {
    p -= offset;
    return p;
}

inline ptrdiff_t operator-(VirtualPointer p, VirtualPointer q) {
    return static_cast<ptrdiff_t>(p.offset) - static_cast<ptrdiff_t>(q.offset);
}

inline bool isAligned(VirtualPointer p, size_t align) {
    return p.offset % align == 0;
}

static_assert(sizeof(VirtualPointer) == 8,
              "Pointers must be exactly 8 bytes in size because we expose them "
              "as uint64_t");

} // namespace svm

#endif // SVM_VIRTUALPOINTER_H_
