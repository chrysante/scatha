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

    app.add_option("file", result.filepath, "Program to execute")
        ->check(CLI::ExistingFile);

    try {
        app.parse(argc, argv);
        result.time = !!*time;
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }

    return result;
}
