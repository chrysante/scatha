#include "Util.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/IRParser.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/PassManager.h>
#include <scatha/Opt/Optimizer.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
#include <svm/VirtualMachine.h>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>

using namespace scatha;

std::optional<ScathaData> scatha::parseScatha(
    std::span<SourceFile const> sourceFiles) {
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

std::optional<ScathaData> scatha::parseScatha(OptionsBase const& options) {
    auto sourceFiles = options.files |
                       ranges::views::transform([](auto const& path) {
                           return SourceFile::load(path);
                       }) |
                       ranges::to<std::vector>;
    return parseScatha(sourceFiles);
}

std::pair<ir::Context, ir::Module> scatha::parseIR(OptionsBase const& options) {
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

void scatha::optimize(ir::Context& ctx,
                      ir::Module& mod,
                      OptionsBase const& options) {
    if (options.optLevel >= 0) {
        opt::optimize(ctx, mod, options.optLevel);
    }
    else if (!options.pipeline.empty()) {
        auto pipeline = ir::PassManager::makePipeline(options.pipeline);
        pipeline(ctx, mod);
    }
}

extern const utl::vstreammanip<> scatha::Warning = [](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
};

extern const utl::vstreammanip<> scatha::Error = [](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
};
