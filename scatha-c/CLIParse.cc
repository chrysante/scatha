#include "CLIParse.h"

#include <cstdlib>

#include <CLI/CLI11.hpp>

using namespace scathac;

Options scathac::parseCLI(int argc, char* argv[]) {
    CLI::App app{ "scatha-compiler" };
    std::filesystem::path filepath, objpath;
    app.add_option("-f,--file", filepath, "Input filename");
    app.add_option("--objdir", objpath, "Object filename");
    CLI::Option const& time = *app.add_flag("-t,--time", "Measure duration of compilation and running");
    try {
        app.parse(argc, argv);
        return { .filepath = filepath, .objpath = objpath, .time = !!time };
    }
    catch (CLI::ParseError const& e) {
        int const exitCode = app.exit(e);
        std::exit(exitCode);
    }
}
