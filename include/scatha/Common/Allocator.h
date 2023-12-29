#ifndef SCATHA_COMMON_ALLOCATOR_H_
#define SCATHA_COMMON_ALLOCATOR_H_

#include <memory>
#include <span>

#include <scatha/Common/Base.h>

namespace scatha {

/// "Arena" allocator. Allocation increases a pointer in the current memory
/// block or allocates a new block. New blocks grow geometrically in size.
/// Deallocation is a no-op. Memory gets freed when the allocator is destroyed
/// or when `release()` is called.
class SCATHA_API MonotonicBufferAllocator {
public:
    /// Default value for the size of the first allocated block
    static constexpr size_t InititalSize = 128;

public:
    MonotonicBufferAllocator();
    explicit MonotonicBufferAllocator(size_t initSize);
    MonotonicBufferAllocator(MonotonicBufferAllocator&&) noexcept;
    ~MonotonicBufferAllocator();
    MonotonicBufferAllocator& operator=(MonotonicBufferAllocator&&) noexcept;

    /// Allocate \p size number of bytes aligned to boundary specified by \p
    /// align
    void* allocate(size_t size, size_t align);

    /// Deallocate \p ptr
    /// This function is a no-op and only exists for symmetry with `allocate()`
    void deallocate(void*, size_t, size_t) {}

    /// Releases the buffer chain and dellocates all memory
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
    u8* current = nullptr;
    u8* end = nullptr;
};

/// Allocates memory for object of type `T` using the allocator \p alloc and
/// constructs the object with arguments \p args... \Returns a pointer the the
/// constructed object
template <typename T, typename... Args>
    requires requires(Args&&... args) { T{ std::forward<Args>(args)... }; }
T* allocate(MonotonicBufferAllocator& alloc, Args&&... args) {
    T* result = static_cast<T*>(alloc.allocate(sizeof(T), alignof(T)));
    std::construct_at(result, std::forward<Args>(args)...);
    return result;
}

/// Allocates memory for an array of element type `T` with \p count elements
/// using the allocator \p alloc Does not construct the elements
template <typename T>
T* allocateArrayUninit(MonotonicBufferAllocator& alloc, size_t count) {
    T* result = static_cast<T*>(alloc.allocate(count * sizeof(T), alignof(T)));
    return result;
}

/// Allocates memory for an array of element type `T` with \p count elements
/// using the allocator \p alloc and default constructs the elements
template <typename T, typename Itr>
std::span<T> allocateArray(MonotonicBufferAllocator& alloc,
                           Itr begin,
                           Itr end) {
    size_t const count = std::distance(begin, end);
    T* result = allocateArrayUninit<T>(alloc, count);
    std::uninitialized_copy(begin, end, result);
    return { result, count };
}

} // namespace scatha

inline void* operator new(size_t size,
                          scatha::MonotonicBufferAllocator& alloc) {
    return alloc.allocate(size, alignof(std::max_align_t));
}

namespace scatha::internal {
/// Exposed in the header to be able to test it
SCATHA_API u8* alignPointer(u8* ptr, size_t alignment);
} // namespace scatha::internal

#endif // SCATHA_COMMON_ALLOCATOR_H_
