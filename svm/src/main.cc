#include <chrono>
#include <fstream>
#include <iostream>

#include <array>
#include <span>
#include <string>
#include <string_view>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/ParseCLI.h>
#include <svm/Util.h>
#include <svm/VirtualMachine.h>
#include <svm/VirtualPointer.h>
#include <utl/format_time.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

using namespace svm;

int main(int argc, char* argv[]) {
    try {
        Options options = parseCLI(argc, argv);
        std::string progName = options.filepath.stem();
        VirtualMachine vm;
        auto binary = readBinaryFromFile(options.filepath.string());
        if (binary.empty()) {
            std::cerr << "Failed to run " << progName << ". Binary is empty.\n";
            return -1;
        }
        vm.setLibdir(options.filepath.parent_path());
        vm.loadBinary(binary.data());

        /// Setup arguments on the stack
        auto execArg = setupArguments(vm, options.arguments);
        /// Excute the program
        auto const beginTime = std::chrono::high_resolution_clock::now();
        vm.execute(execArg);
        auto const endTime = std::chrono::high_resolution_clock::now();
        u64 const exitCode = vm.getRegister(0);
        if (options.time) {
            std::cout << "Execution took "
                      << utl::format_duration(endTime - beginTime) << "\n";
        }
        return static_cast<int>(exitCode);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
