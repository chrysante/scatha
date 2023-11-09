#include <chrono>
#include <fstream>
#include <iostream>

#include <array>
#include <span>
#include <string>
#include <string_view>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/VirtualMachine.h>
#include <svm/VirtualPointer.h>
#include <utl/format_time.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CLIParse.h"

using namespace svm;

/// Seeks the scatha binary in the file \p file to ignore any prepended bash
/// commands See documentation of `writeBashHeader()` in "scatha-c" for an
/// explanation of the convention
std::span<uint8_t const> seekBinary(std::span<uint8_t const> file) {
    auto* data = file.data();
    auto* end = std::to_address(file.end());
    /// We ignore any empty lines
    while (data < end && *data == '\n') {
        ++data;
    }
    /// We ignore lines starting with `#` and the next line
    while (data < end && *data == '#') {
        for (int i = 0; i < 2; ++i) {
            while (data < end && *data != '\n') {
                ++data;
            }
            ++data;
        }
    }
    return std::span(data, end);
}

struct ArgRange {
    size_t offset, size;
};

struct Arguments {
    utl::small_vector<ArgRange> pointers;
    utl::vector<u8> data;
};

static Arguments generateArguments(std::span<std::string const> args) {
    Arguments result;
    for (auto& arg: args) {
        result.pointers.push_back({ result.data.size(), arg.size() });
        ranges::copy(arg, std::back_inserter(result.data));
    }
    return result;
}

int main(int argc, char* argv[]) {
    Options options = parseCLI(argc, argv);
    std::string progName = options.filepath.stem();
    std::fstream file(options.filepath, std::ios::in);
    if (!file) {
        std::cerr << "Failed to open program: " << options.filepath
                  << std::endl;
        return -1;
    }
    file.seekg(0, std::ios::end);
    auto const filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    utl::vector<u8> executable(static_cast<size_t>(filesize));
    file.read(reinterpret_cast<char*>(executable.data()), filesize);

    if (executable.empty()) {
        std::cerr << "Failed to run " << progName << ". Binary is empty.\n";
        return -1;
    }

    VirtualMachine vm;
    try {
        vm.loadBinary(seekBinary(executable).data());
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    /// Copy arguments onto the stack below the stack frame of `main()`
    auto arguments = generateArguments(options.arguments);
    size_t argPointersSize = arguments.pointers.size() * 16;
    size_t stringDataSize = arguments.data.size();
    size_t totalArgSize = argPointersSize + stringDataSize;
    VirtualPointer argStackData = vm.allocateStackMemory(totalArgSize);
    struct StringPointer {
        VirtualPointer ptr;
        uint64_t size;
    };
    auto argPointers =
        arguments.pointers | ranges::views::transform([&](auto arg) {
            return StringPointer{ argStackData + argPointersSize + arg.offset,
                                  arg.size };
        }) |
        ranges::to<utl::small_vector<StringPointer>>;
    std::memcpy(vm.derefPointer(argStackData, argPointersSize),
                argPointers.data(),
                argPointersSize);
    std::memcpy(vm.derefPointer(argStackData + argPointersSize, stringDataSize),
                arguments.data.data(),
                stringDataSize);
    std::array<u64, 2> actualArgument = { std::bit_cast<u64>(argStackData),
                                          argPointers.size() };

    /// Excute the program
    auto const beginTime = std::chrono::high_resolution_clock::now();
    vm.execute(actualArgument);
    auto const endTime = std::chrono::high_resolution_clock::now();
    u64 const exitCode = vm.getRegister(0);
    if (options.time) {
        std::cout << "Execution took "
                  << utl::format_duration(endTime - beginTime) << "\n";
    }
    return static_cast<int>(exitCode);
}
