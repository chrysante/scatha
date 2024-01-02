#include <unordered_map>

#include <CLI/CLI11.hpp>

#include "Compiler.h"
#include "Graph.h"
#include "Inspect.h"
#include "Options.h"
#include "Util.h"

using namespace scatha;

static void commonOptions(CLI::App* app, OptionsBase& opt) {
    app->add_option("files", opt.files)->check(CLI::ExistingFile);
    app->add_option("-L, --libsearchpaths", opt.libSearchPaths);
    static std::unordered_map<std::string, TargetType> const targetTypeMap = {
        { "exec", TargetType::Executable },
        { "staticlib", TargetType::StaticLibrary },
    };
    app->add_option("-T,--target-type", opt.targetType, "Target type")
        ->transform(CLI::CheckedTransformer(targetTypeMap, CLI::ignore_case));
    app->add_option("-O,--output", opt.outputFile, "Directory to place binary");
}

int main(int argc, char* argv[]) {
    CLI::App compiler("sctool");
    compiler.require_subcommand(0, 1);

    // clang-format off
    CompilerOptions compilerOptions{};
    commonOptions(&compiler, compilerOptions);
    compiler.add_flag_callback("-o,--optimize", [&]{ compilerOptions.optLevel = 1; }, "Optimize the program");
    compiler.add_flag("-d,--debug", compilerOptions.debug, "Generate debug symbols");
    compiler.add_flag("-t,--time", compilerOptions.time, "Measure compilation time");
    compiler.add_flag("-b, --binary-only", compilerOptions.binaryOnly,
                  "Emit .sbin file. Otherwise the compiler emits an executable that can be run directly using a shell script hack");
    
    CLI::App* inspect = compiler.add_subcommand("inspect", "Tool to visualize the state of the compilation pipeline");
    InspectOptions inspectOptions{};
    commonOptions(inspect, inspectOptions);
    inspect->add_flag("--ast", inspectOptions.ast, "Print AST");
    inspect->add_flag("--sym", inspectOptions.sym, "Print symbol table");
    inspect->add_option("--pipeline", inspectOptions.pipeline, "Optimization pipeline to be run on the IR");
    inspect->add_flag("--emit-ir", inspectOptions.emitIR, "Write generated IR to file");
    inspect->add_flag("--isel", inspectOptions.isel, "Run the experimental ISel pipeline");
    inspect->add_flag("--codegen", inspectOptions.codegen, "Print codegen pipeline");
    inspect->add_flag("--asm", inspectOptions.assembly, "Print assembly");
    
    CLI::App* graph = compiler.add_subcommand("graph", "Tool to generate images of various graphs in the compilation pipeline");
    GraphOptions graphOptions{};
    commonOptions(graph, graphOptions);
    graph->add_option("--pipeline", graphOptions.pipeline, "Optimization pipeline to be run on the IR");
    graph->add_option("--dest", graphOptions.dest, "Directory to write the generated files");
    graph->add_flag("--svg", graphOptions.generatesvg, "Generate SVG files");
    graph->add_flag("--open", graphOptions.open, "Open generated graphs")->needs("--svg");
    graph->add_flag("--cfg", graphOptions.cfg, "Draw control flow graph");
    graph->add_flag("--calls", graphOptions.calls, "Draw call graph");
    graph->add_flag("--interference", graphOptions.interference, "Draw interference graph");
    graph->add_flag("--selection-dag", graphOptions.selectiondag, "Draw selection DAG");
    // clang-format on

    try {
        compiler.parse(argc, argv);
        if (inspect->parsed()) {
            return inspectMain(inspectOptions);
        }
        if (graph->parsed()) {
            return graphMain(graphOptions);
        }
        return compilerMain(compilerOptions);
    }
    catch (CLI::ParseError const& e) {
        return compiler.exit(e);
    }
    catch (std::exception const& e) {
        std::cout << Error << e.what() << std::endl;
        return 1;
    }
}
