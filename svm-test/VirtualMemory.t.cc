#include <Catch/Catch2.hpp>

#include <algorithm>
#include <random>

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
    auto staticDataBegin = VirtualMemory::MakeStaticDataPointer(0);
    CHECK_NOTHROW(mem.dereference(staticDataBegin, 128));
    CHECK_THROWS(mem.dereference(staticDataBegin, 129));

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

TEST_CASE("Virtual memory allocations and deallocations") {
    static constexpr std::array<size_t, 3> alignments = { 4, 8, 16 };
    size_t seed = GENERATE(0u, 123u, 12456434u, 7564534u);
    std::mt19937_64 rng(seed);
    std::vector<int> runs;
    std::generate_n(std::back_inserter(runs), 10, [&] {
        return std::uniform_int_distribution<int>(10, 30)(rng);
    });
    runs.reserve(2 * runs.size());
    std::copy(runs.begin(), runs.end(), std::back_inserter(runs));
    std::shuffle(runs.begin(), runs.end(), rng);
    VirtualMemory mem(128);
    for (int run: runs) {
        struct Block {
            VirtualPointer ptr;
            size_t size;
            size_t align;
        };
        std::vector<Block> blocks;
        for (int i = 0; i < run; ++i) {
            size_t size = std::uniform_int_distribution<size_t>(5, 10'000)(rng);
            size_t alignIdx =
                std::uniform_int_distribution<size_t>(0, alignments.size() - 1)(
                    rng);
            size_t align = alignments[alignIdx];
            auto ptr = mem.allocate(size, align);
            blocks.push_back({ ptr, size, align });
        }
        for (auto block: blocks) {
            CHECK_NOTHROW(mem.deallocate(block.ptr, block.size, block.align));
        }
    }
}

TEST_CASE("Virtual memory fuzz invalid accesses") {
    size_t seed = GENERATE(0u, 123u, 7564534u);
    size_t numAllocs = GENERATE(0u, 1u, 1000u);
    std::mt19937_64 rng(seed);
    std::vector<std::pair<size_t, size_t>> sizes;
    std::generate_n(std::back_inserter(sizes), numAllocs, [&] {
        static constexpr std::array<size_t, 3> Aligns = { 4, 8, 16 };
        std::uniform_int_distribution<size_t> sizeDist(10, 2000);
        std::uniform_int_distribution<size_t> alignDist(0, Aligns.size() - 1);
        return std::pair{ sizeDist(rng), Aligns[alignDist(rng)] };
    });
    VirtualMemory mem;
    for (auto [size, align]: sizes) {
        (void)mem.allocate(size, align);
    }
    std::uniform_int_distribution<size_t> range(0, 1000);
    for (size_t i = 0; i < 1'000; ++i) {
        try {
            mem.dereference(std::bit_cast<VirtualPointer>(rng()), range(rng));
        }
        catch (MemoryAccessError const& e) {
            (void)e;
        }
    }
}

TEST_CASE("Virtual memory deallocate invalid pointer") {
    VirtualMemory mem(128);
    auto ptr = mem.allocate(32, 8);
    /// Deallocate the 32 byte block as 8 bytes
    CHECK_THROWS_AS(mem.deallocate(ptr, 8, 8), DeallocationError);
}
