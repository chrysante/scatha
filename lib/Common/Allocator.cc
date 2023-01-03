#include "Common/Allocator.h"

#include <cstdlib>

#include <utl/utility.hpp>

#include "Basic/Basic.h"

namespace scatha {

u8* internal::alignPointer(u8* ptr, size_t alignment) {
    static_assert(sizeof(size_t) == sizeof(ptr));
    size_t const r = utl::fast_mod_pow_two(reinterpret_cast<size_t>(ptr), alignment);
    ptr += alignment * !!r - r;
    return ptr;
}

MonotonicBufferAllocator::MonotonicBufferAllocator() {}

MonotonicBufferAllocator::MonotonicBufferAllocator(size_t initSize) {
    addChunk(initSize);
}

MonotonicBufferAllocator::MonotonicBufferAllocator(MonotonicBufferAllocator&& rhs) noexcept:
    buffer(rhs.buffer), current(rhs.current), end(rhs.end) {
    rhs.buffer  = nullptr;
    rhs.current = nullptr;
    rhs.end     = nullptr;
}

MonotonicBufferAllocator::~MonotonicBufferAllocator() {
    release();
}

MonotonicBufferAllocator& MonotonicBufferAllocator::operator=(MonotonicBufferAllocator&& rhs) noexcept {
    release();
    buffer      = rhs.buffer;
    current     = rhs.current;
    end         = rhs.end;
    rhs.buffer  = nullptr;
    rhs.current = nullptr;
    rhs.end     = nullptr;
    return *this;
}

SCATHA(DISABLE_UBSAN) /// Disable UBSan for method \p allocate  as it may
                      /// perform pointer arithmetic on \p nullptr. This pointer
                      /// will be never dereferenced though, so its all fine.
void* MonotonicBufferAllocator::allocate(size_t size, size_t align) {
    using namespace internal;
    u8* const result = alignPointer(current, align);
    u8* const next   = result + size;
    if (next > end) {
        addChunk(buffer ? buffer->size * 2 : inititalSize);
        return allocate(size, align);
    }
    current = next;
    return result;
}

void MonotonicBufferAllocator::release() {
    InternalBufferHeader* buf = buffer;
    while (buf) {
        size_t const size                = buf->size;
        InternalBufferHeader* const prev = buf->prev;
        std::free(buf);
        (void)size;
        buf = prev;
    }
    buffer  = nullptr;
    current = nullptr;
    end     = nullptr;
}

void MonotonicBufferAllocator::addChunk(size_t size) {
    InternalBufferHeader* const newBuffer =
        static_cast<InternalBufferHeader*>(std::malloc(size + sizeof(InternalBufferHeader)));
    newBuffer->prev = buffer;
    newBuffer->size = size;

    buffer  = newBuffer;
    current = reinterpret_cast<u8*>(newBuffer) + sizeof(InternalBufferHeader);
    end     = current + size;
}

} // namespace scatha
