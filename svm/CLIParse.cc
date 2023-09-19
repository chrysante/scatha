#include "CLIParse.h"

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace svm;

Options svm::parseCLI(int argc, char* argv[]) {
    Options result;
    CLI::App app{ "Scatha Virtual Machine" };
    std::filesystem::path filepath, objpath;
    CLI::Option const* time =
        app.add_flag("-t,--time", "Measure execution time");
    app.add_option("arguments",
                   result.arguments,
                   "Executable file and arguments to pass to the executable");
    try {
        app.parse(argc, argv);
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
    result.time = !!*time;
    if (result.arguments.empty()) {
        std::cout << "No executable\n";
        std::exit(-1);
    }
    result.filepath = result.arguments.front();
    if (!std::filesystem::exists(result.filepath)) {
        std::cout << "Not a file: " << result.filepath << "\n";
        std::exit(-1);
    }
    result.arguments.erase(result.arguments.begin());
    return result;
}
