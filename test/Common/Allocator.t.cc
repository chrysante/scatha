#include <catch2/catch_test_macros.hpp>

#include "Common/Allocator.h"

using namespace scatha;

TEST_CASE("alignPointer") {
    using namespace internal;
    u8* p;
    p = reinterpret_cast<u8*>(16);
    CHECK(alignPointer(p, 1) == p);
    CHECK(alignPointer(p, 4) == p);
    CHECK(alignPointer(p, 8) == p);
    CHECK(alignPointer(p, 16) == p);
    p = reinterpret_cast<u8*>(4);
    CHECK(alignPointer(p, 1) == p);
    CHECK(alignPointer(p, 4) == p);
    CHECK(alignPointer(p, 8) == p + 4);
    CHECK(alignPointer(p, 16) == p + 12);
    p = reinterpret_cast<u8*>(3);
    CHECK(alignPointer(p, 1) == p);
    CHECK(alignPointer(p, 4) == p + 1);
    CHECK(alignPointer(p, 8) == p + 5);
    CHECK(alignPointer(p, 16) == p + 13);
}

TEST_CASE("MonotonicBufferAllocator") {
    MonotonicBufferAllocator alloc(128);
    for (int i = 0; i < 100; ++i) {
        u8* ptr = static_cast<u8*>(alloc.allocate(16, 8));
        CHECK(reinterpret_cast<size_t>(ptr) % 8 == 0);
        std::memset(ptr, 0, 16);
    }
}
