#include "CLIParse.h"

#include <iostream>
#include <vector>

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace scathac;

Options scathac::parseCLI(int argc, char* argv[]) {
    Options result;
    CLI::App app("Scatha Compiler");
    auto* opt = app.add_flag("-o,--optimize", "Optimize the program");
    auto* debug = app.add_flag("-d,--debug", "Generate debug symbols");
    auto* time = app.add_flag("-t,--time", "Measure compilation time");
    auto* binonly = app.add_flag("-b, --binary-only",
                                 "Emit .sbin file. Otherwise the compiler "
                                 "emits an executable that can be run directly "
                                 "using a shell script hack");
    app.add_option("--out-dir", result.bindir, "Directory to place binary");
    app.add_option("files", result.files, "Input files")
        ->check(CLI::ExistingPath);
    try {
        app.parse(argc, argv);
        result.optimize = !!*opt;
        result.time = !!*time;
        result.binaryOnly = !!*binonly;
        result.debug = !!debug;
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
    return result;
}
