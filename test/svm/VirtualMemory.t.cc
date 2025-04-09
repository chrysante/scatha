#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <algorithm>
#include <functional>
#include <random>

#include <svm/VirtualMemory.h>
#include <utl/utility.hpp>

using namespace svm;

TEST_CASE("Static data", "[virtual-memory]") {
    VirtualMemory mem(128);
    auto staticDataBegin = VirtualMemory::MakeStaticDataPointer(0);
    CHECK_NOTHROW(mem.dereference(staticDataBegin, 128));
    CHECK_THROWS(mem.dereference(staticDataBegin, 129));
}

TEST_CASE("Virtual memory", "[virtual-memory]") {
    auto [size, align] = GENERATE(std::pair<size_t, size_t>{ 4, 4 },
                                  std::pair<size_t, size_t>{ 8, 8 },
                                  std::pair<size_t, size_t>{ 16, 8 },
                                  std::pair<size_t, size_t>{ 16, 16 },
                                  std::pair<size_t, size_t>{ 19, 8 },
                                  std::pair<size_t, size_t>{ 30, 8 },
                                  std::pair<size_t, size_t>{ 32, 8 },
                                  std::pair<size_t, size_t>{ 2000, 8 });
    size_t roundedSize = utl::round_up(size, align);
    VirtualMemory mem(128);
    SECTION("Single allocation") {
        auto ptr = mem.allocate(roundedSize, align);
        mem.derefAs<int>(ptr, size) = 1;
        CHECK(mem.derefAs<int>(ptr, size) == 1);
        mem.deallocate(ptr, roundedSize, align);
    }
    SECTION("Consecutive allocations") {
        std::vector<VirtualPointer> ptrs;

        for (int i = 0; i < 100; ++i) {
            ptrs.push_back(mem.allocate(roundedSize, align));
            mem.derefAs<int>(ptrs.back(), size) = i;
        }
        int sum = 0;
        for (auto ptr: ptrs) {
            sum += mem.derefAs<int>(ptr, size);
        }
        for (auto ptr: ptrs) {
            mem.deallocate(ptr, roundedSize, align);
        }
        int n = static_cast<int>(ptrs.size()) - 1;
        CHECK(sum == n * (n + 1) / 2);
    }
}

static size_t alignTo(size_t size, size_t align) {
    return size % align == 0 ? size : size + align - size % align;
}

TEST_CASE("Allocations and deallocations", "[virtual-memory]") {
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
            size_t alignIdx =
                std::uniform_int_distribution<size_t>(0, alignments.size() -
                                                             1)(rng);
            size_t align = alignments[alignIdx];
            size_t size =
                alignTo(std::uniform_int_distribution<size_t>(5, 10'000)(rng),
                        align);
            auto ptr = mem.allocate(size, align);
            blocks.push_back({ ptr, size, align });
        }
        for (auto block: blocks) {
            CHECK_NOTHROW(mem.deallocate(block.ptr, block.size, block.align));
        }
    }
}

TEST_CASE("Allocations and mappings intermingled", "[virtual-memory]") {
    size_t const seed = Catch::Generators::random(size_t{ 0 }, SIZE_MAX).get();
    auto randomIndex = [rng = std::mt19937_64(seed)](size_t size) mutable {
        return std::uniform_int_distribution<size_t>(0, size - 1)(rng);
    };
    VirtualMemory mem(128);
    using Action = std::function<void()>;
    auto hostMemoryRegions = [&] {
        std::vector<std::vector<char>> regions;
        for (size_t i = 0; i < 100; ++i) {
            regions.emplace_back(1 + randomIndex(1000));
        }
        return regions;
    }();
    struct Block {
        VirtualPointer ptr;
        size_t size;
        size_t align;
    };
    std::vector<Block> allocatedBlocks;
    std::vector<size_t> mappedSlots;
    auto makeAction = [&]() -> Action {
        switch (randomIndex(4)) {
        case 0: // Allocate memory
            return [&] {
                size_t align = GENERATE(1, 2, 4, 8);
                size_t size = align * randomIndex(1000);
                allocatedBlocks.push_back({ .ptr = mem.allocate(size, align),
                                            .size = size,
                                            .align = align });
            };
        case 1: // Deallocate memory
            return [&] {
                if (allocatedBlocks.empty()) {
                    return;
                }
                auto itr = allocatedBlocks.begin() +
                           randomIndex(allocatedBlocks.size());
                mem.deallocate(itr->ptr, itr->size, itr->align);
                allocatedBlocks.erase(itr);
            };
        case 2: // Map memory
            return [&] {
                auto& region =
                    hostMemoryRegions[randomIndex(hostMemoryRegions.size())];
                auto p = mem.map(region.data(), region.size());
                mappedSlots.push_back(p.slotIndex);
            };
        case 3: // Unmap memory
            return [&] {
                if (mappedSlots.empty()) {
                    return;
                }
                auto itr =
                    mappedSlots.begin() + randomIndex(mappedSlots.size());
                mem.unmap(*itr);
                mappedSlots.erase(itr);
            };
        default:
            assert(false);
        }
    };
    auto actions = [&] {
        std::vector<Action> actions;
        for (size_t i = 0; i < 10000; ++i) {
            actions.push_back(makeAction());
        }
        return actions;
    }();
    for (auto& action: actions) {
        CHECK_NOTHROW(action());
    }
}

TEST_CASE("Fuzz invalid accesses", "[virtual-memory]") {
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
        (void)mem.allocate(utl::round_up(size, align), align);
    }
    std::uniform_int_distribution<size_t> range(0, 1000);
    for (size_t i = 0; i < 1'000; ++i) {
        try {
            mem.dereference(std::bit_cast<VirtualPointer>(rng()), range(rng));
        }
        catch (RuntimeException const& e) {
            (void)e;
        }
    }
}

TEST_CASE("Deallocate invalid pointer", "[virtual-memory]") {
    VirtualMemory mem(128);
    auto ptr = mem.allocate(32, 8);
    /// Deallocate the 32 byte block as 8 bytes
    CHECK_THROWS_AS(mem.deallocate(ptr, 8, 8), RuntimeException);
}

TEST_CASE("Zero size allocation", "[virtual-memory]") {
    VirtualMemory mem(128);
    mem.allocate(8, 8);
    auto ptr = mem.allocate(0, 8);
    CHECK_NOTHROW([&] { mem.deallocate(ptr, 0, 8); }());
}
