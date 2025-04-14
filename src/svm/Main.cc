#include <chrono>
#include <fstream>
#include <iostream>

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include <CLI/CLI.hpp>
#include <scbinutil/ProgramView.h>
#include <svm/Util.h>
#include <svm/VirtualMachine.h>
#include <svm/VirtualPointer.h>
#include <utl/format_time.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

using namespace svm;

namespace {

struct Options {
    std::filesystem::path filepath;
    std::vector<std::string> arguments;
    bool time;
    bool noJumpThread;
};

} // namespace

static int firstArgIndex(int argc, char* argv[]) {
    for (int i = 0; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!arg.starts_with("--binary")) {
            continue;
        }
        if (arg != "--binary" && arg != "--binary=") {
            return i + 1;
        }
        if (i + 1 == argc) {
            std::cout << "Expected file argument after --binary.\n";
            std::exit(1);
        }
        return i + 2;
    }
    return argc;
}

static Options parseOptions(int argc, char* argv[]) {
    int const optionsArgC = firstArgIndex(argc, argv);
    Options result{};
    CLI::App app{ "Scatha Virtual Machine" };
    std::filesystem::path filepath;
    app.add_flag("-t,--time", result.time, "Measure execution time");
    app.add_flag("--no-jump-thread", result.noJumpThread,
                 "Don't use jump threading for execution");
    app.add_option("--binary", result.filepath, "Executable file")
        ->check(CLI::ExistingFile);
    try {
        app.parse(optionsArgC, argv);
    }
    catch (CLI::ParseError const& e) {
        std::exit(app.exit(e));
    }
    for (int i = optionsArgC; i < argc; ++i)
        result.arguments.push_back(argv[i]);
    return result;
}

int main(int argc, char* argv[]) {
    try {
        Options options = parseOptions(argc, argv);
        std::string progName = options.filepath.stem().string();
        VirtualMachine vm;
        std::vector<uint8_t> binary =
            readBinaryFromFile(options.filepath.string());
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
        if (!options.noJumpThread) {
            vm.execute(execArg);
        }
        else {
            vm.executeNoJumpThread(execArg);
        }
        auto const endTime = std::chrono::high_resolution_clock::now();
        u64 const exitCode = vm.getRegister(0);
        if (options.time) {
            std::clog << "Execution took "
                      << utl::format_duration(endTime - beginTime) << "\n";
        }
        return static_cast<int>(exitCode);
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
