#include "Parser.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <scatha/AST/LowerToIR.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Parser.h>
#include <scatha/IR/Print.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
#include <scatha/Sema/SymbolTable.h>

#include "Util.h"

using namespace scatha;
using namespace passtool;

std::pair<ir::Context, ir::Module> passtool::parseFile(
    std::filesystem::path path, ParseMode mode) {
    std::fstream file(path, std::ios::in);
    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = std::move(sstr).str();

    using enum ParseMode;
    if (mode == Default) {
        if (path.extension() == ".sc") {
            mode = Scatha;
        }
        if (path.extension() == ".scir") {
            mode = IR;
        }
    }
    switch (mode) {
    case Scatha: {
        IssueHandler issueHandler;
        auto ast = parse::parse(text, issueHandler);
        if (!issueHandler.empty()) {
            issueHandler.print(text);
            issueHandler.clear();
        }
        if (issueHandler.haveErrors()) {
            std::exit(-1);
        }
        sema::SymbolTable semaSym;
        auto analysisResult = sema::analyze(*ast, semaSym, issueHandler);
        if (!issueHandler.empty()) {
            issueHandler.print(text);
        }
        if (issueHandler.haveErrors()) {
            std::exit(-1);
        }
        return ast::lowerToIR(*ast, semaSym, analysisResult);
    }
    case IR: {
        auto result = ir::parse(text);
        if (result) {
            return std::move(result.value());
        }
        std::cout << Error << "Failed to parse " << path << std::endl;
        ir::print(result.error());
        std::exit(-1);
    }
    case Default: {
        std::cout << Error << "Unknown file extension: " << path.extension()
                  << std::endl;
        std::exit(-1);
    }
    }
}
