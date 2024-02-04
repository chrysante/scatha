#ifndef SVM_VIRTUALMEMORY_H_
#define SVM_VIRTUALMEMORY_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <svm/Errors.h>
#include <svm/VirtualPointer.h>

namespace svm {

/// A slot in the `VirtualMemory` class
struct Slot {
public:
    /// Construct an emply slot
    Slot() = default;

    /// Construct a slot with an initial size \p initSize
    Slot(size_t initSize): buffer(initSize) {}

    /// The raw pointer to the beginning of the buffer
    char* data() { return buffer.data(); }

    /// The size of the buffer
    size_t size() const { return buffer.size(); }

    /// Set the size of the buffer to \p size
    void resize(size_t size) { buffer.resize(size); }

    /// Grow the buffer by a geometric factor
    void grow(size_t minSize) {
        buffer.resize(std::max(minSize, buffer.size() * 2));
    }

private:
    std::vector<char> buffer;
};

/// An allocator for small block sizes used internally by `VirtualMemory`
class PoolAllocator {
public:
    /// Construct a pool allocator for blocks of size \p blockSize
    explicit PoolAllocator(size_t blockSize);

    /// Allocates a block of memory in the slot \p slot
    /// \Returns the offset of the allocated memory block from the beginning of
    /// the slot \p slot
    size_t allocate(Slot& slot);

    /// Adds the block at offset \p offset to the freelist
    bool deallocate(Slot& slot, size_t offset);

    /// The block size this pool is responsible for
    size_t blockSize() const { return blkSize; }

private:
    size_t blkSize;
    size_t freelistBegin = 0;
};

/// Represents an unbounded region of memory from which we can allocate blocks.
/// The first slot is the 'static slot' where we allocate static data, byte code
/// and stack memory
class VirtualMemory {
public:
    /// Create a pointer to static data from an offset
    static VirtualPointer MakeStaticDataPointer(size_t offset);

    /// Construct a virtual memory region with a static block size of \p
    /// staticSlotSize
    explicit VirtualMemory(size_t staticSlotSize = 0);

    /// Allocates a block of memory of size \p size and alignment \p align
    /// \p align must be a power of two
    /// \p size must be evenly divisible of \p align
    VirtualPointer allocate(size_t size, size_t align);

    /// Deallocates the block at address \p ptr
    void deallocate(VirtualPointer ptr, size_t size, size_t align);

    ///
    void resizeStaticSlot(size_t size);
    /// \Returns the number of bytes at which the pointer \p ptr is
    /// dereferencable. If the pointer is not valid a negative number is
    /// returned
    ptrdiff_t validRange(VirtualPointer ptr) const;

    /// Converts the virtual pointer \p ptr to an actual pointer. \p size is the
    /// amount of bytes at which \p ptr will be dereferencable
    /// \Note Null pointers are not valid values for \p ptr
    void* dereference(VirtualPointer ptr, size_t size);

    /// Converts the native pointer \p ptr to its host representation.
    /// \p ptr may be null
    void* nativeToHost(VirtualPointer ptr);

    /// Converts the pointer \p ptr to a reference to type `T`
    template <typename T>
    T& derefAs(VirtualPointer ptr, size_t size);

private:
    /// \Returns pair of slot index and pool reference responsible for
    /// allocating blocks of size \p size
    std::pair<size_t, PoolAllocator&> getPool(size_t size, size_t align);

    /// \Throws a `MemoryAccessError` unconditionally
    [[noreturn, gnu::noinline]] static void reportAccessError(
        MemoryAccessError::Reason reason, VirtualPointer ptr, size_t size);

    /// \Throws a `DeallocationError` unconditionally
    [[noreturn, gnu::noinline]] static void reportDeallocationError(
        VirtualPointer ptr, size_t size, size_t align);

    std::vector<Slot> slots;
    std::vector<PoolAllocator> pools;
    std::vector<size_t> freeSlots;
};

} // namespace svm

/// # Inline implementation

inline ptrdiff_t svm::VirtualMemory::validRange(VirtualPointer ptr) const {
    if (ptr.slotIndex == 0 || ptr.slotIndex >= slots.size()) {
        return -1;
    }
    auto& slot = slots[ptr.slotIndex];
    return ptrdiff_t(slot.size()) - ptrdiff_t(ptr.offset);
}

inline void* svm::VirtualMemory::dereference(VirtualPointer ptr, size_t size) {
    using enum MemoryAccessError::Reason;
    if (ptr.slotIndex == 0 || ptr.slotIndex >= slots.size()) {
        reportAccessError(MemoryNotAllocated, ptr, size);
    }
    auto& slot = slots[ptr.slotIndex];
    if (ptr.offset + size > slot.size()) {
        reportAccessError(DerefRangeTooBig, ptr, size);
    }
    return slot.data() + ptr.offset;
}

inline void* svm::VirtualMemory::nativeToHost(VirtualPointer ptr) {
    using enum MemoryAccessError::Reason;
    if (ptr == VirtualPointer::Null) {
        return nullptr;
    }
    if (ptr.slotIndex == 0 || ptr.slotIndex >= slots.size()) {
        reportAccessError(MemoryNotAllocated, ptr, ~size_t(0));
    }
    auto& slot = slots[ptr.slotIndex];
    return slot.data() + ptr.offset;
}

template <typename T>
T& svm::VirtualMemory::derefAs(VirtualPointer ptr, size_t size) {
    void* raw = dereference(ptr, size);
    assert(reinterpret_cast<uintptr_t>(raw) % alignof(T) == 0 &&
           "Misaligned pointer");
    return *reinterpret_cast<T*>(raw);
}

#endif // SVM_VIRTUALMEMORY_H_
