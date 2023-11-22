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

sema::StructType const* Compiler::declareType(StructDesc desc) {
    auto* type = sym.declareStructureType(desc.name);
    assert(false && "Unimplemented");
    return type;
}

sema::Type const* Compiler::mapType(std::type_info const& key,
                                    sema::Type const* value) {
    auto [itr, success] = typeMap.insert({ key, value });
    assert(success);
    return itr->second;
}

sema::Type const* Compiler::mapType(std::type_info const& key,
                                    StructDesc valueDesc) {
    return mapType(key, declareType(std::move(valueDesc)));
}

static size_t const FunctionSlot = 16;

FuncDecl Compiler::declareFunction(std::string name,
                                   sema::FunctionSignature signature) {
    size_t slot = FunctionSlot;
    size_t index = functionIndex++;
    auto* function = sym.declareForeignFunction(name,
                                                slot,
                                                index,
                                                std::move(signature),
                                                sema::FunctionAttribute::None);
    return { .function = function, .slot = slot, .index = index };
}

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
    sema::SymbolTable semaSym;
    auto analysisResult = sema::analyze(*ast, semaSym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (issueHandler.haveErrors()) {
        throw std::runtime_error("Compilation failed");
    }
    auto [context, mod] = irgen::generateIR(*ast, semaSym, analysisResult, {});
    opt::optimize(context, mod, 1);
    auto asmStream = cg::codegen(mod);
    auto [data, sym] = Asm::assemble(asmStream);
    Program prog;
    std::tie(prog._data, prog._sym) =
        std::tuple{ std::move(data), std::move(sym) };
    return prog;
}
