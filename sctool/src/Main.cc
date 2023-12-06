#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI11.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <scatha/AST/Print.h>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/CodeGen/Passes.h>
#include <scatha/CodeGen/SelectionDAG.h>
#include <scatha/Common/ExecutableWriter.h>
#include <scatha/Common/Logging.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Graphviz.h>
#include <scatha/IR/IRParser.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/PassManager.h>
#include <scatha/IR/Print.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/MIR/Context.h>
#include <scatha/MIR/Module.h>
#include <scatha/MIR/Print.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
#include <scatha/Sema/Print.h>
#include <scatha/Sema/SymbolTable.h>
#include <svm/VirtualMachine.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

using namespace scatha;

struct OptionsBase {
    std::vector<std::filesystem::path> files;
    std::string pipeline;
};

enum class Mode { Scatha, IR };

static Mode getMode(OptionsBase const& options) {
    if (options.files.empty()) {
        throw std::runtime_error("No input files");
    }
    auto hasExt = [](std::string ext) {
        return [=](auto& path) { return path.extension() == ext; };
    };
    if (ranges::all_of(options.files, hasExt(".sc"))) {
        return Mode::Scatha;
    }
    if (ranges::all_of(options.files, hasExt(".scir")) &&
        options.files.size() == 1)
    {
        return Mode::IR;
    }
    if (options.files.size() <= 1) {
        throw std::runtime_error("Invalid file extension");
    }
    else {
        throw std::runtime_error("Invalid combination of file extensions");
    }
}

struct ScathaData {
    UniquePtr<ast::ASTNode> ast;
    sema::SymbolTable sym;
    sema::AnalysisResult analysisResult;
};

static std::optional<ScathaData> parseScatha(OptionsBase const& options) {
    auto sourceFiles = options.files |
                       ranges::views::transform([](auto const& path) {
                           return SourceFile::load(path);
                       }) |
                       ranges::to<std::vector>;
    IssueHandler issueHandler;
    ScathaData result;
    result.ast = parser::parse(sourceFiles, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (!result.ast) {
        return std::nullopt;
    }
    result.analysisResult =
        sema::analyze(*result.ast, result.sym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (issueHandler.haveErrors()) {
        return std::nullopt;
    }
    return result;
}

static std::pair<ir::Context, ir::Module> parseIR(OptionsBase const& options) {
    assert(options.files.size() == 1);
    auto path = options.files.front();
    std::fstream file(path);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open file ", path));
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    auto result = ir::parse(std::move(sstr).str());
    if (!result) {
        std::stringstream sstr;
        ir::print(result.error(), sstr);
        throw std::runtime_error(sstr.str());
    }
    return std::move(result).value();
}

struct InspectOptions: OptionsBase {
    bool ast;
    bool sym;
    bool emitIR;
    bool codegen;
    bool isel; // Experimental
    bool assembly;
    bool execute;
    bool out;
};

static void reportRetval(uint64_t retval) {
    auto const signedRetval = static_cast<int64_t>(retval);
    // clang-format off
    std::cout << "Program returned: " << retval;
    std::cout << "\n                 (0x" << std::hex << retval << std::dec << ")";
    if (signedRetval < 0) {
    std::cout << "\n                 (" << signedRetval << ")";
    }
    std::cout << "\n                 (" << std::bit_cast<double>(retval) << ")";
    std::cout << std::endl;
    // clang-format on
}

/// The `execute*()` functions are not `static` so they can be executed from a
/// debugger
void executeAsm(Asm::AssemblyStream const& asmStream) {
    auto [program, symbolTable] = Asm::assemble(asmStream);
    svm::VirtualMachine vm;
    vm.loadBinary(program.data());
    vm.execute({});
    reportRetval(vm.getRegister(0));
}

/// See `executeAsm()`
void executeIR(ir::Module const& mod) { executeAsm(cg::codegen(mod)); }

static int inspectMain(InspectOptions options) {
    using namespace logging;
    ir::Context ctx;
    ir::Module mod;
    switch (getMode(options)) {
    case Mode::Scatha: {
        auto data = parseScatha(options);
        if (!data) {
            return 1;
        }
        if (options.ast) {
            header("AST");
            ast::printTree(*data->ast);
        }
        if (options.sym) {
            header("Symbol Table");
            sema::print(data->sym);
        }
        std::tie(ctx, mod) =
            irgen::generateIR(*data->ast,
                              data->sym,
                              data->analysisResult,
                              { .generateDebugSymbols = false });
        break;
    }
    case Mode::IR:
        std::tie(ctx, mod) = parseIR(options);
        break;
    }
    if (!options.pipeline.empty()) {
        auto pipeline = ir::PassManager::makePipeline(options.pipeline);
        pipeline(ctx, mod);
    }
    if (options.emitIR) {
        if (auto file =
                std::fstream("out.scir", std::ios::out | std::ios::trunc))
        {
            ir::print(mod, file);
        }
        else {
            std::cout << "Failed to write \"out.scir\"" << std::endl;
        }
    }
    if (options.isel) {
        mir::Context ctx;
        auto mirMod = cg::lowerToMIR2(ctx, mod);
        std::cout << "Warning: Other codegen options and execution are ignored "
                     "with the --isel flag"
                  << std::endl;
        header("Generated MIR");
        mir::print(mirMod);
        return 0;
    }
    auto cgLogger = [&]() -> std::unique_ptr<cg::Logger> {
        if (options.codegen) {
            return std::make_unique<cg::DebugLogger>();
        }
        return std::make_unique<cg::NullLogger>();
    }();
    auto asmStream = cg::codegen(mod, *cgLogger);
    if (options.assembly) {
        header("Assembly");
        Asm::print(asmStream);
    }
    if (options.execute) {
        header("Execution");
        executeAsm(asmStream);
    }
    if (options.out) {
        auto [program, symbolTable] = Asm::assemble(asmStream);
        writeExecutableFile("out", program);
    }
    return 0;
}

struct GraphOptions: OptionsBase {
    std::filesystem::path dest;
    bool generatesvg;
    bool open;
    bool cfg;
    bool calls;
    bool interference;
    bool selectiondag;
};

static std::fstream openFile(std::filesystem::path path) {
    std::fstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open file ", path));
    }
    return file;
}

static int graphMain(GraphOptions options) {
    ir::Context ctx;
    ir::Module mod;
    switch (getMode(options)) {
    case Mode::Scatha: {
        auto data = parseScatha(options);
        if (!data) {
            return 1;
        }
        std::tie(ctx, mod) =
            irgen::generateIR(*data->ast,
                              data->sym,
                              data->analysisResult,
                              { .generateDebugSymbols = false });
        break;
    }
    case Mode::IR:
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

int main(int argc, char** argv) {
    CLI::App root("sctool");
    root.require_subcommand(1, 1);

    // clang-format off
    CLI::App* inspect = root.add_subcommand("inspect", "Tool to visualize the state of the compilation pipeline");
    InspectOptions inspectOptions{};
    inspect->add_option("files", inspectOptions.files)->check(CLI::ExistingPath);
    inspect->add_flag("--ast", inspectOptions.ast, "Print AST");
    inspect->add_flag("--sym", inspectOptions.sym, "Print symbol table");
    inspect->add_option("--pipeline", inspectOptions.pipeline, "Optimization pipeline to be run on the IR");
    inspect->add_flag("--emit-ir", inspectOptions.emitIR, "Write generated IR to file");
    inspect->add_flag("--isel", inspectOptions.isel, "Run the experimental ISel pipeline");
    inspect->add_flag("--codegen", inspectOptions.codegen, "Print codegen pipeline");
    inspect->add_flag("--asm", inspectOptions.assembly, "Print assembly");
    inspect->add_flag("--execute", inspectOptions.execute, "Execute the compiled program");
    inspect->add_flag("--out", inspectOptions.out, "Emit executable file");
    
    CLI::App* graph = root.add_subcommand("graph", "Tool to generate images of various graphs in the compilation pipeline");
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
        root.parse(argc, argv);
        if (inspect->parsed()) {
            return inspectMain(inspectOptions);
        }
        if (graph->parsed()) {
            return graphMain(graphOptions);
        }
    }
    catch (CLI::ParseError const& e) {
        return root.exit(e);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
