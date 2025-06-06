#include "svm/VirtualMemory.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

using namespace svm;

static constexpr size_t roundUp(size_t value, size_t multipleOf) {
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

Slot Slot::Owning(size_t initSize) {
    return Slot((char*)std::malloc(initSize), initSize, true);
}

Slot Slot::View(void* buffer, size_t size) {
    return Slot((char*)buffer, size, false);
}

void Slot::resize(size_t size) {
    assert(owning);
    auto* newbuf = (char*)std::malloc(size);
    std::memcpy(newbuf, buf, std::min(sz, size));
    std::free(buf);
    buf = newbuf;
    sz = size;
}

void Slot::grow(size_t minSize) { resize(std::max(minSize, size() * 2)); }

void Slot::clear() {
    if (owning) {
        std::free(buf);
    }
    buf = nullptr;
    sz = 0;
    owning = false;
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
    if (blockSize() == 0) {
        return true;
    }
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

/// Slot index 0 is unused and invalid. This way the null pointer is trivially
/// invalid

/// Static data and stack memory goes into slot index 1.
static constexpr size_t StaticDataIndex = 1;

/// Pools start at index 2
static constexpr size_t FirstPoolIndex = 2;

/// Difference between two block sizes
static constexpr size_t BlockSizeDiff = 16;

/// The maximum allocation size up to which we allocate using pools
static constexpr size_t MaxPoolSize = 1024;

/// \Returns the index of the pool that is responsible for managing block of
/// size \p size and align \p align This takes the slots below the pool slots
/// into account
static constexpr size_t toPoolIndex(size_t size, size_t) {
    size_t index = roundUp(size, BlockSizeDiff) / BlockSizeDiff;
    index += FirstPoolIndex - 1;
    return index;
}

///
static constexpr size_t LastPoolIndex = toPoolIndex(MaxPoolSize, 1);

static VirtualPointer const ZeroSizedAllocationResult = { .offset = 0,
                                                          .slotIndex = 1 };

VirtualPointer VirtualMemory::MakeStaticDataPointer(size_t offset) {
    return { .offset = offset, .slotIndex = StaticDataIndex };
}

VirtualMemory::VirtualMemory(size_t staticDataSize) {
    slots.reserve(2 + MaxPoolSize / BlockSizeDiff);
    /// Index 0 is unsued
    slots.push_back(Slot::Owning(0));
    /// Static data
    slots.push_back(Slot::Owning(staticDataSize));
    /// Pools
    for (size_t i = BlockSizeDiff; i <= MaxPoolSize; i += BlockSizeDiff) {
        slots.push_back(Slot::Owning(0));
        pools.push_back(PoolAllocator(i));
    }
}

VirtualPointer VirtualMemory::allocate(size_t size, size_t align) {
    if (size == 0) {
        return ZeroSizedAllocationResult;
    }
    if (size >= uint64_t(1) << 48) {
        throwError<AllocationError>(AllocationError::InvalidSize, size, align);
    }
    if (std::popcount(align) != 1 || size % align != 0) {
        throwError<AllocationError>(AllocationError::InvalidAlign, size, align);
    }
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
        slots.push_back(Slot::Owning(size));
        assert(slotIndex < (1 << 16) && "Maximum slot number exceeded");
        return { .offset = 0, .slotIndex = slotIndex };
    }
}

void VirtualMemory::deallocate(VirtualPointer ptr, size_t size, size_t align) {
    if (size == 0) {
        if (ptr != ZeroSizedAllocationResult) {
            reportDeallocationError(ptr, size, align);
        }
        return;
    }
    if (std::popcount(align) != 1) {
        reportDeallocationError(ptr, size, align);
    }
    if (size <= MaxPoolSize) {
        auto [slotIndex, pool] = getPool(size, align);
        if (slotIndex != ptr.slotIndex) {
            reportDeallocationError(ptr, size, align);
        }
        if (!pool.deallocate(slots[slotIndex], ptr.offset)) {
            reportDeallocationError(ptr, size, align);
        }
        return;
    }
    if (ptr.slotIndex <= LastPoolIndex) {
        reportDeallocationError(ptr, size, align);
    }
    freeSlots.push_back(ptr.slotIndex);
}

void VirtualMemory::resizeStaticSlot(size_t size) {
    slots[StaticDataIndex].resize(size);
}

std::pair<size_t, PoolAllocator&> VirtualMemory::getPool(size_t size,
                                                         size_t align) {
    assert(size <= MaxPoolSize);
    size_t index = toPoolIndex(size, align);
    return { index, pools[index - FirstPoolIndex] };
}

void VirtualMemory::reportAccessError(MemoryAccessError::Reason reason,
                                      VirtualPointer ptr, size_t size) {
    throwError<MemoryAccessError>(reason, ptr, size);
}

void VirtualMemory::reportDeallocationError(VirtualPointer ptr, size_t size,
                                            size_t align) {
    throwError<DeallocationError>(ptr, size, align);
}

VirtualPointer VirtualMemory::map(void* p, size_t size) {
    size_t slotIndex = [this](auto makeSlot) {
        if (!freeSlots.empty()) {
            size_t slotIndex = freeSlots.back();
            freeSlots.pop_back();
            slots[slotIndex] = makeSlot();
            return slotIndex;
        }
        else {
            size_t slotIndex = slots.size();
            slots.push_back(makeSlot());
            return slotIndex;
        }
    }([&] { return Slot::View(p, size); });
    return VirtualPointer{ .offset = 0, .slotIndex = slotIndex };
}

void VirtualMemory::unmap(size_t slotIndex) {
    slots[slotIndex] = Slot::Owning(0);
    freeSlots.push_back(slotIndex);
}
