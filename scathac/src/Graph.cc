#include "Graph.h"

#include <fstream>
#include <iostream>

#include <scatha/IR/Context.h>
#include <scatha/IR/Graphviz.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/PassManager.h>
#include <scatha/IRGen/IRGen.h>
#include <utl/strcat.hpp>

#include "Util.h"

using namespace scatha;

static std::fstream openFile(std::filesystem::path path) {
    std::fstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open file ", path));
    }
    return file;
}

int scatha::graphMain(GraphOptions options) {
    ir::Context ctx;
    ir::Module mod;
    switch (deduceFrontend(options.files)) {
    case FrontendType::Scatha: {
        auto data = parseScatha(options);
        if (!data) {
            return 1;
        }
        std::tie(ctx, mod) = genIR(*data->ast, data->sym, data->analysisResult,
                                   { .generateDebugSymbols = false });
        break;
    }
    case FrontendType::IR:
        std::tie(ctx, mod) = parseIR(options);
        break;
    }
    auto pipeline = ir::PassManager::makePipeline(options.pipeline);
    pipeline(ctx, mod);

    auto generate = [&](std::filesystem::path const& gvPath) {
        auto svgPath = gvPath;
        svgPath.replace_extension(".svg");
        if (options.generatesvg) {
            std::system(
                utl::strcat("dot -Tsvg ", gvPath, " -o ", svgPath).c_str());
        }
        if (options.open) {
            std::system(utl::strcat("open ", svgPath).c_str());
        }
    };

    if (options.cfg) {
        auto path = options.dest / "cfg.gv";
        auto file = openFile(path);
        ir::generateGraphviz(mod, file);
        file.close();
        generate(path);
    }
    if (options.calls) {
        std::cout << "Drawing call graph is not implemented" << std::endl;
    }
    if (options.interference) {
        std::cout << "Drawing interference graph is not implemented"
                  << std::endl;
    }
    if (options.selectiondag) {
        std::cout << "Drawing selection DAG is not implemented" << std::endl;
    }
    return 0;
}
