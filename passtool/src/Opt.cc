#include "Opt.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <scatha/IR/Print.h>

#include "Parser.h"
#include "Util.h"

using namespace scatha;
using namespace passtool;

Opt::Opt(CLI::App* parent): Command(parent, "opt") {
    app()->add_option("files", paths);
    app()->add_option("-o, --out", outdir);
}

int Opt::run() {
    if (paths.empty()) {
        std::cout << Error << "No input files" << std::endl;
        return -1;
    }
    auto path = paths.front();
    auto [ctx, mod] = parseFile(path);

    if (outdir.empty()) {
        outdir = path;
        outdir.replace_extension(".out.scir");
    }
    std::stringstream sstr;
    ir::print(mod, sstr);
    std::fstream outfile(outdir, std::ios::out | std::ios::trunc);
    if (!outfile) {
        std::cout << Error << "Failed to write to file: " << outdir
                  << std::endl;
        return -1;
    }
    outfile << sstr.str();
    return 0;
}
