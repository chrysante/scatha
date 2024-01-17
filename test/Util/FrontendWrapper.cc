#include "Util/FrontendWrapper.h"

#include <span>
#include <sstream>
#include <stdexcept>

#include "Common/SourceFile.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IRGen/IRGen.h"
#include "Issue/IssueHandler.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace test;

static void validateEmpty(std::span<SourceFile const> sources,
                          IssueHandler const& issues) {
    if (issues.empty()) {
        return;
    }
    std::stringstream sstr;
    issues.print(sources, sstr);
    throw std::runtime_error(sstr.str());
}

std::pair<ir::Context, ir::Module> test::makeIR(
    std::vector<std::string> sourceTexts) {
    IssueHandler issues;
    auto sourceFiles = sourceTexts | ranges::views::transform([](auto& text) {
        return SourceFile::make(std::move(text));
    }) | ranges::to<std::vector>;
    auto ast = parser::parse(sourceFiles, issues);
    validateEmpty(sourceFiles, issues);
    sema::SymbolTable sym;
    auto analysisResult = sema::analyze(*ast, sym, issues);
    validateEmpty(sourceFiles, issues);
    ir::Context ctx;
    ir::Module mod;
    irgen::generateIR(ctx, mod, *ast, sym, analysisResult, {});
    return { std::move(ctx), std::move(mod) };
}
