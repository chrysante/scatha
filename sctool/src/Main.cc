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
#include <scatha/Common/Logging.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Graphviz.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Parser.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Opt/PassManager.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
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

struct PrintOptions: OptionsBase {
    bool ast;
    bool codegen;
    bool assembly;
    bool execute;
};

static int printMain(PrintOptions options) {
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
    header("Generated IR");
    auto pipeline = opt::PassManager::makePipeline(options.pipeline);
    pipeline(ctx, mod);
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
        auto [program, symbolTable] = Asm::assemble(asmStream);
        svm::VirtualMachine VM;
        VM.loadBinary(program.data());
        VM.execute({});
    }
    return 0;
}

struct GraphOptions: OptionsBase {
    bool cfg;
    bool calls;
    bool interference;
    bool selectiondag;
};

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
    auto pipeline = opt::PassManager::makePipeline(options.pipeline);
    pipeline(ctx, mod);

    if (options.cfg) {
        // ...
    }
    if (options.calls) {
        // ...
    }
    if (options.interference) {
        // ...
    }
    if (options.selectiondag) {
        // ...
    }
    return 0;
}

int main(int argc, char** argv) {
    CLI::App root;
    root.require_subcommand(1, 1);

    // clang-format off
    CLI::App* print = root.add_subcommand("print");
    PrintOptions printOptions{};
    print->add_option("files", printOptions.files);
    print->add_flag("--ast", printOptions.ast, "Print AST");
    print->add_option("--pipeline", printOptions.pipeline, "Optimization pipelien to be run on the IR");
    print->add_flag("--codegen", printOptions.codegen, "Print codegen pipeline");
    print->add_flag("--asm", printOptions.assembly, "Print assembly");
    print->add_flag("--execute", printOptions.execute, "Execute the compiled program");

    CLI::App* graph = root.add_subcommand("graph");
    GraphOptions graphOptions{};
    graph->add_option("files", graphOptions.files);
    graph->add_option("--pipeline", graphOptions.pipeline, "Optimization pipelien to be run on the IR");
    graph->add_flag("--cfg", graphOptions.cfg, "Draw control flow graph");
    graph->add_flag("--calls", graphOptions.calls, "Draw call graph");
    graph->add_flag("--interference", graphOptions.interference, "Draw interference graph");
    graph->add_flag("--selection-dag", graphOptions.selectiondag, "Draw selection DAG");
    // clang-format on

    try {
        root.parse(argc, argv);
        if (print->parsed()) {
            return printMain(printOptions);
        }
        if (graph->parsed()) {
            return graphMain(graphOptions);
        }
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
