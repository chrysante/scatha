#include <chrono>
#include <fstream>
#include <iostream>

#include <svm/VirtualMachine.h>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CLIParse.h"

using namespace svm;

int main(int argc, char* argv[]) {
    Options options = parseCLI(argc, argv);

    std::fstream file(options.filepath, std::ios::in);
    if (!file) {
        std::cout << "Failed to open program: " << options.filepath
                  << std::endl;
        return -1;
    }
    file.seekg(0, std::ios::end);
    auto const filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    utl::vector<u8> program(static_cast<size_t>(filesize));
    file.read(reinterpret_cast<char*>(program.data()), filesize);

    VirtualMachine vm;
    vm.loadProgram(program.data());

    auto const beginTime = std::chrono::high_resolution_clock::now();
    vm.execute();
    auto const endTime = std::chrono::high_resolution_clock::now();
    u64 const exitCode = vm.getState().registers[0];
    std::cout << "Program returned with exit code: " << exitCode << std::endl;

    if (options.time) {
        auto const dur = endTime - beginTime;
        std::cout << "Execution took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                         .count()
                  << "ms." << std::endl;
    }
}
