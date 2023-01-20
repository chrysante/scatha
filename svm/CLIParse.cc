#include "CLIParse.h"

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace svm;

Options svm::parseCLI(int argc, char* argv[]) {
    CLI::App app{ "Scatha Virtual Machine" };
    std::filesystem::path filepath, objpath;
    app.add_option("-f,--file", filepath, "Program to execute");
    CLI::Option const& time = *app.add_flag("-t,--time", "Measure duration of run");
    try {
        app.parse(argc, argv);
        return { .filepath = filepath, .time = !!time };
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
}
