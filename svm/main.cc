#include <chrono>
#include <fstream>
#include <iostream>

#include <svm/VirtualMachine.h>
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
    vm.loadBinary(seekBinary(executable).data());

    if (!options.arguments.empty()) {
        std::cout << "Passed arguments are: ";
        for (auto& arg: options.arguments) {
            std::cout << arg << " ";
        }
        std::cout << "\n"
                  << "Warning: For now arguments are ignored and not passed to "
                     "main.\n";
    }

    auto const beginTime = std::chrono::high_resolution_clock::now();
    vm.execute({});
    auto const endTime = std::chrono::high_resolution_clock::now();
    u64 const exitCode = vm.getRegister(0);
    if (options.time) {
        std::cout << "Execution took "
                  << utl::format_duration(endTime - beginTime) << "\n";
    }
    return static_cast<int>(exitCode);
}
