#include "svm/VirtualMemory.h"

using namespace svm;

MemoryError::MemoryError(std::string message, VirtualPointer pointer):
    runtime_error(std::move(message)), ptr(pointer) {}

static size_t roundUp(size_t value, size_t multipleOf) {
    size_t rem = value % multipleOf;
    if (rem == 0) {
        return value;
    }
    return value + multipleOf - rem;
}

template <typename T>
static T read(void const* memory) {
    T t;
    std::memcpy(&t, memory, sizeof(T));
    return t;
}

template <typename T>
static void write(void* memory, std::type_identity_t<T> t) {
    std::memcpy(memory, &t, sizeof(T));
}

PoolAllocator::PoolAllocator(size_t blockSize): blkSize(blockSize) {
    assert(blockSize >= 8 &&
           "We need at least 8 byte block size to maintain the freelist");
}

size_t PoolAllocator::allocate(Slot& slot) {
    if (slot.size() == freelistBegin) {
        slot.grow(2 * blockSize());
        /// Create a freelist in the newly allocated memory
        for (size_t i = freelistBegin; i < slot.size(); i += blockSize()) {
            write<size_t>(slot.data() + i, i + blockSize());
        }
    }
    size_t offset = freelistBegin;
    freelistBegin = read<size_t>(slot.data() + freelistBegin);
    return offset;
}

bool PoolAllocator::deallocate(Slot& slot, size_t offset) {
    if (offset % blockSize() != 0) {
        return false;
    }
    if (offset >= slot.size()) {
        return false;
    }
    write<size_t>(slot.data() + offset, freelistBegin);
    freelistBegin = offset;
    return true;
}

/// Below the pools we reserve one slot for the binary and for the stack
static constexpr size_t FirstPoolIndex = 1;

/// Difference between two block sizes
static constexpr size_t BlockSizeDiff = 16;

/// The maximum allocation size up to which we allocate using pools
static constexpr size_t MaxPoolSize = 1024;

/// \Returns the index of the pool that is responsible for managing block of
/// size \p size and align \p align This takes the slots below the pool slots
/// into account
static size_t toPoolIndex(size_t size, size_t align) {
    size_t index = roundUp(size, BlockSizeDiff) / BlockSizeDiff;
    index += FirstPoolIndex - 1;
    return index;
}

VirtualMemory::VirtualMemory(size_t firstSlotSize) {
    for (size_t i = 0; i < FirstPoolIndex; ++i) {
        slots.emplace_back(firstSlotSize);
    }
    for (size_t i = BlockSizeDiff; i <= MaxPoolSize; i += BlockSizeDiff) {
        slots.emplace_back();
        pools.push_back(PoolAllocator(i));
    }
}

VirtualPointer VirtualMemory::allocate(size_t size, size_t align) {
    assert(std::popcount(align) == 1 && "Not a power of two");
    if (size <= MaxPoolSize) {
        auto [slotIndex, pool] = getPool(size, align);
        size_t offset = pool.allocate(slots[slotIndex]);
        return { .offset = offset, .slotIndex = slotIndex };
    }
    if (!freeSlots.empty()) {
        size_t slotIndex = freeSlots.back();
        freeSlots.pop_back();
        auto& slot = slots[slotIndex];
        if (size > slot.size()) {
            slot.grow(size);
        }
        return { .offset = 0, .slotIndex = slotIndex };
    }
    else {
        size_t slotIndex = slots.size();
        slots.emplace_back(size);
        assert(slotIndex < (1 << 16) && "Maximum slot number exceeded");
        return { .offset = 0, .slotIndex = slotIndex };
    }
}

void VirtualMemory::deallocate(VirtualPointer ptr, size_t size, size_t align) {
    assert(std::popcount(align) == 1 && "Not a power of two");
    if (size <= MaxPoolSize) {
        auto [slotIndex, pool] = getPool(size, align);
        if (slotIndex != ptr.slotIndex) {
            throw DeallocationError(ptr, size, align);
        }
        if (!pool.deallocate(slots[slotIndex], ptr.offset)) {
            throw DeallocationError(ptr, size, align);
        }
        return;
    }
    freeSlots.push_back(ptr.slotIndex);
}

void VirtualMemory::resizeStaticSlot(size_t size) { slots[0].resize(size); }

std::pair<size_t, PoolAllocator&> VirtualMemory::getPool(size_t size,
                                                         size_t align) {
    assert(size <= MaxPoolSize);
    size_t index = toPoolIndex(size, align);
    return { index, pools[index - FirstPoolIndex] };
}

void VirtualMemory::reportAccessError(MemoryAccessError::Reason reason,
                                      VirtualPointer ptr,
                                      size_t size) {
    throw MemoryAccessError(reason, ptr, size);
}