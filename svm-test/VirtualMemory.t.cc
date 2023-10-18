#include <Catch/Catch2.hpp>

#include "VirtualMemory.h"

using namespace svm;

TEST_CASE("Virtual memory") {
    auto [size, align] = GENERATE(std::pair<size_t, size_t>{ 4, 4 },
                                  std::pair<size_t, size_t>{ 8, 8 },
                                  std::pair<size_t, size_t>{ 16, 8 },
                                  std::pair<size_t, size_t>{ 16, 16 },
                                  std::pair<size_t, size_t>{ 19, 8 },
                                  std::pair<size_t, size_t>{ 30, 8 },
                                  std::pair<size_t, size_t>{ 32, 8 },
                                  std::pair<size_t, size_t>{ 2000, 8 });
    VirtualMemory mem(128);
    CHECK_NOTHROW(mem.dereference(VirtualPointer{ 0, 0 }, 128));
    CHECK_THROWS(mem.dereference(VirtualPointer{ 0, 0 }, 129));

    SECTION("Single allocation") {
        auto ptr = mem.allocate(size, align);
        mem.derefAs<int>(ptr, size) = 1;
        CHECK(mem.derefAs<int>(ptr, size) == 1);
        mem.deallocate(ptr, size, align);
    }
    SECTION("Consecutive allocations") {
        std::vector<VirtualPointer> ptrs;
        for (int i = 0; i < 100; ++i) {
            ptrs.push_back(mem.allocate(size, align));
            mem.derefAs<int>(ptrs.back(), size) = i;
        }
        int sum = 0;
        for (auto ptr: ptrs) {
            sum += mem.derefAs<int>(ptr, size);
        }
        for (auto ptr: ptrs) {
            mem.deallocate(ptr, size, align);
        }
        int n = static_cast<int>(ptrs.size()) - 1;
        CHECK(sum == n * (n + 1) / 2);
    }
}

TEST_CASE("Virtual memory deallocate invalid pointer") {
    VirtualMemory mem(128);
    auto ptr = mem.allocate(32, 8);
    /// Deallocate the 32 byte block as 8 bytes
    CHECK_THROWS_AS(mem.deallocate(ptr, 8, 8), DeallocationError);
}
