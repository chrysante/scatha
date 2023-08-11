#include <filesystem>
#include <iostream>
#include <vector>

#include <CLI/CLI11.hpp>

#include "Opt.h"
#include "REPL.h"
#include "Util.h"

using namespace scatha;
using namespace passtool;

int main(int argc, char** argv) {
    CLI::App app("passtool");
    app.require_subcommand(1, 1);

    REPL repl(&app);
    Opt opt(&app);

    try {
        app.parse(argc, argv);
    }
    catch (CLI::ParseError const& e) {
        std::exit(app.exit(e));
    }

    if (repl) {
        return repl.run();
    }
    if (opt) {
        return opt.run();
    }
}
