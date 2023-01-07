#include "CLIParse.h"

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace scathac;

Options scathac::parseCLI(int argc, char* argv[]) {
    CLI::App app{ "scatha-compiler" };
    CLI::Option const& run = *app.add_flag("-r,--run", "Run the program after successfull compilation");
    std::filesystem::path filepath;
    app.add_option("-f,--file", filepath, "Filename");
    try {
        app.parse(argc, argv);
        return { .filepath = filepath, .run = !!run };
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
}
