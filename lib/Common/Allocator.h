#ifndef SCATHA_COMMON_ALLOCATOR_H_
#define SCATHA_COMMON_ALLOCATOR_H_

#include <memory>
#include <span>

#include "Basic/Basic.h"

namespace scatha {

class SCATHA(API) MonotonicBufferAllocator {
public:
    static constexpr size_t inititalSize = 128;

public:
    MonotonicBufferAllocator();
    explicit MonotonicBufferAllocator(size_t initSize);
    MonotonicBufferAllocator(MonotonicBufferAllocator&&) noexcept;
    ~MonotonicBufferAllocator();
    MonotonicBufferAllocator& operator=(MonotonicBufferAllocator&&) noexcept;
    void* allocate(size_t size, size_t align);
    void deallocate(void*, size_t, size_t) { /* no-op */
    }

    void release();

private:
    struct InternalBufferHeader {
        InternalBufferHeader* prev;
        size_t size;
    };

private:
    void addChunk(size_t size);

private:
    InternalBufferHeader* buffer = nullptr;
    u8* current                  = nullptr;
    u8* end                      = nullptr;
};

template <typename T, typename... Args>
requires requires(Args&&... args) { T{ std::forward<Args>(args)... }; }
T* allocate(MonotonicBufferAllocator& alloc, Args&&... args) {
    T* result = static_cast<T*>(alloc.allocate(sizeof(T), alignof(T)));
    std::construct_at(result, std::forward<Args>(args)...);
    return result;
}

template <typename T>
T* allocateArrayUninit(MonotonicBufferAllocator& alloc, size_t count) {
    T* result = static_cast<T*>(alloc.allocate(count * sizeof(T), alignof(T)));
    return result;
}

template <typename T, typename Itr>
std::span<T> allocateArray(MonotonicBufferAllocator& alloc, Itr begin, Itr end) {
    size_t const count = std::distance(begin, end);
    T* result          = allocateArrayUninit<T>(alloc, count);
    std::uninitialized_copy(begin, end, result);
    return { result, count };
}

} // namespace scatha

inline void* operator new(size_t size, scatha::MonotonicBufferAllocator& alloc) {
    return alloc.allocate(size, alignof(std::max_align_t));
}

namespace scatha::internal {
// expose it to the header to be able to test it
SCATHA(API) u8* alignPointer(u8* ptr, size_t alignment);
} // namespace scatha::internal

#endif // SCATHA_COMMON_ALLOCATOR_H_
