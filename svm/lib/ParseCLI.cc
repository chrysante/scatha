#include <svm/ParseCLI.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

#include <CLI/CLI.hpp>

using namespace svm;

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

Options svm::parseCLI(int argc, char* argv[]) {
    int const optionsArgC = firstArgIndex(argc, argv);
    Options result{};
    CLI::App app{ "Scatha Virtual Machine" };
    std::filesystem::path filepath;
    app.add_flag("-t,--time", result.time, "Measure execution time");
    app.add_flag("--print", result.print, "Print the binary");
    app.add_option("--binary", result.filepath, "Executable file")
        ->check(CLI::ExistingFile);
    try {
        app.parse(optionsArgC, argv);
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
    for (int i = optionsArgC; i < argc; ++i) {
        result.arguments.push_back(argv[i]);
    }
    return result;
}
