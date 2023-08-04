#include "CLIParse.h"

#include <iostream>
#include <vector>

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace scathac;

Options scathac::parseCLI(int argc, char* argv[]) {
    Options result;

    CLI::App app("Scatha Compiler");
    CLI::Option const* opt =
        app.add_flag("-o,--optimize", "Optimize the program");

    CLI::Option const* time =
        app.add_flag("-t,--time", "Measure compilation time");

    app.add_option("-b,--bindir", result.bindir, "Directory to place binary");

    app.add_option("files", result.files, "Input files")
        ->check(CLI::ExistingPath);

    try {
        app.parse(argc, argv);
        result.optimize = !!*opt;
        result.time = !!*time;
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }

    return result;
}
