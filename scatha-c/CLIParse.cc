#include "CLIParse.h"

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace scathac;

Options scathac::parseCLI(int argc, char* argv[]) {
    CLI::App app{ "scatha-compiler" };
    std::filesystem::path filepath;
    app.add_option("-f,--file", filepath, "Filename");
    CLI::Option const& run  = *app.add_flag("-r,--run", "Run the program after successfull compilation");
    CLI::Option const& time = *app.add_flag("-t,--time", "Measure duration of compilation and running");
    try {
        app.parse(argc, argv);
        return { .filepath = filepath, .run = !!run, .time = !!time };
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
}
