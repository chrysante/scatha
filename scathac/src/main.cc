#include <CLI/CLI11.hpp>

#include "Compiler.h"
#include "Graph.h"
#include "Inspect.h"
#include "Options.h"
#include "Util.h"

using namespace scatha;

int main(int argc, char* argv[]) {
    CLI::App compiler("sctool");
    compiler.require_subcommand(0, 1);

    // clang-format off
    CompilerOptions compilerOptions{};
    compiler.add_option("files", compilerOptions.files)->check(CLI::ExistingPath);
    compiler.add_flag("-o,--optimize", compilerOptions.optimize, "Optimize the program");
    compiler.add_flag("-d,--debug", compilerOptions.debug, "Generate debug symbols");
    compiler.add_flag("-t,--time", compilerOptions.time, "Measure compilation time");
    compiler.add_flag("-b, --binary-only", compilerOptions.binaryOnly,
                  "Emit .sbin file. Otherwise the compiler emits an executable that can be run directly using a shell script hack");
    compiler.add_option("--out-dir", compilerOptions.bindir, "Directory to place binary");
    
    CLI::App* inspect = compiler.add_subcommand("inspect", "Tool to visualize the state of the compilation pipeline");
    InspectOptions inspectOptions{};
    inspect->add_option("files", inspectOptions.files)->check(CLI::ExistingPath);
    inspect->add_flag("--ast", inspectOptions.ast, "Print AST");
    inspect->add_flag("--sym", inspectOptions.sym, "Print symbol table");
    inspect->add_option("--pipeline", inspectOptions.pipeline, "Optimization pipeline to be run on the IR");
    inspect->add_flag("--emit-ir", inspectOptions.emitIR, "Write generated IR to file");
    inspect->add_flag("--isel", inspectOptions.isel, "Run the experimental ISel pipeline");
    inspect->add_flag("--codegen", inspectOptions.codegen, "Print codegen pipeline");
    inspect->add_flag("--asm", inspectOptions.assembly, "Print assembly");
    inspect->add_option("--out", inspectOptions.out, "Emit executable file");
    
    CLI::App* graph = compiler.add_subcommand("graph", "Tool to generate images of various graphs in the compilation pipeline");
    GraphOptions graphOptions{};
    graph->add_option("files", graphOptions.files)->check(CLI::ExistingPath);
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
