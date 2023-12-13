#include "Runtime/Compiler.h"

#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Opt/Optimizer.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>

#include "Runtime/Program.h"

using namespace scatha;

static size_t const FunctionSlot = 16;

Compiler::Compiler(): lib(sym, FunctionSlot) {}

void Compiler::addSourceText(std::string text, std::filesystem::path path) {
    sourceFiles.push_back(SourceFile::make(std::move(text), std::move(path)));
}

void Compiler::addSourceFile(std::filesystem::path path) {
    sourceFiles.push_back(SourceFile::load(std::move(path)));
}

Program Compiler::compile() {
    IssueHandler issueHandler;
    auto ast = parser::parse(sourceFiles, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (!ast) {
        throw std::runtime_error("Compilation failed");
    }
    auto analysisResult = sema::analyze(*ast, sym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (issueHandler.haveErrors()) {
        throw std::runtime_error("Compilation failed");
    }
    auto [context, mod] = irgen::generateIR(*ast, sym, analysisResult, {});
    opt::optimize(context, mod, 1);
    auto asmStream = cg::codegen(mod);
    auto [bindata, binsym] = Asm::assemble(asmStream);
    Program prog;
    std::tie(prog._data, prog._sym, prog._binsym) =
        std::tuple{ std::move(bindata), std::move(sym), std::move(binsym) };
    return prog;
}
