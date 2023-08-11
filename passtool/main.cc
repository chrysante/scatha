#include <filesystem>
#include <vector>
#include <iostream>
#include <string>

#include <CLI/CLI11.hpp>
#include <utl/streammanip.hpp>
#include <termfmt/termfmt.h>
#include <scatha/AST/LowerToIR.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Print.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
#include <scatha/Sema/SymbolTable.h>
#include <scatha/Opt/PassManager.h>
#include <scatha/Opt/PipelineError.h>
#include <scatha/Opt/Pipeline.h>

using namespace scatha;

static void line(std::string_view m) {
    static constexpr size_t width = 80;
    auto impl = [](size_t width) {
        tfmt::FormatGuard grey(tfmt::BrightGrey);
        for (size_t i = 0; i < width; ++i) {
            std::cout << "=";
        }
    };
    if (m.empty()) {
        impl(width);
        std::cout << std::endl;
    }
    else if (width <= m.size() + 2) {
        std::cout << m << std::endl;
    }
    else {
        size_t outerSpace = width - (m.size() + 2);
        size_t left = outerSpace / 4, right = outerSpace - left;
        impl(left);
        std::cout << " " << tfmt::format(tfmt::Bold, m) << " ";
        impl(right);
        std::cout << std::endl;
    }
};

static void header(std::string_view title) {
    std::cout << "\n";
    line("");
    line(title);
    line("");
    std::cout << "\n";
}

static void subHeader(std::string_view title) {
    std::cout << "\n";
    line(title);
    std::cout << "\n";
}

static constexpr utl::streammanip Warning([](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
});

static constexpr utl::streammanip Error([](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
});

int main(int argc, char** argv) {
    CLI::App app("Scatha Compiler");
    std::vector<std::filesystem::path> files;
    app.add_option("files", files, "Input files")
        ->check(CLI::ExistingPath);
    try {
        app.parse(argc, argv);
    }
    catch (CLI::ParseError const& e) {
        std::exit(app.exit(e));
    }
    if (files.empty()) {
        std::cout << Error << "No input files" << std::endl;
        return -1;
    }

    auto const filepath = files.front();
    std::fstream file(filepath, std::ios::in);
    assert(file && "CLI11 library should have caught this");

    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = std::move(sstr).str();

    IssueHandler issueHandler;
    auto ast = parse::parse(text, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(text);
    }
    if (!ast) {
        return -1;
    }

    /// Analyse the AST
    sema::SymbolTable semaSym;
    auto analysisResult = sema::analyze(*ast, semaSym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(text);
        return -1;
    }

    /// Generate IR
    auto [context, mod] = ast::lowerToIR(*ast, semaSym, analysisResult);
    
    header("Parsed program");
    ir::print(mod);
    
    while (true) {
        std::cout << tfmt::format(tfmt::Bold | tfmt::BrightGrey, ">>") << " ";
        std::string command;
        std::cin >> command;
        if (command == "q" || command == "quit") {
            return 0;
        }
        opt::Pipeline pipeline;
        try {
            pipeline = opt::PassManager::makePipeline(command);
        }
        catch (opt::PipelineError const& error) {
            std::cout << Error << error.what() << std::endl;
            continue;
        }
        bool modified = pipeline(context, mod);
        header("");
        ir::print(mod);
        std::cout << "Modified: " << std::boolalpha << tfmt::format(tfmt::Bold, modified) << std::endl;
    }
}

